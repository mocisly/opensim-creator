#include "CachedModelWarper.h"

#include <libopensimcreator/Documents/CustomComponents/InMemoryMesh.h>
#include <libopensimcreator/Documents/Model/BasicModelStatePair.h>
#include <libopensimcreator/Documents/ModelWarper/IPointWarperFactory.h>
#include <libopensimcreator/Documents/ModelWarper/WarpableModel.h>
#include <libopensimcreator/Graphics/OpenSimDecorationGenerator.h>
#include <libopensimcreator/Utils/OpenSimHelpers.h>
#include <libopensimcreator/Utils/SimTKConverters.h>

#include <liboscar/Formats/OBJ.h>
#include <liboscar/Platform/Log.h>
#include <liboscar/Utils/Assertions.h>
#include <OpenSim/Simulation/Model/Geometry.h>
#include <OpenSim/Simulation/Model/Model.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>

using namespace osc;
using namespace osc::mow;

namespace
{
    std::unique_ptr<OpenSim::Geometry> WarpMesh(
        const WarpableModel& document,
        const OpenSim::Model& model,
        const SimTK::State& state,
        const OpenSim::Mesh& inputMesh,
        const IPointWarperFactory& warper)
    {
        // TODO: this ignores scale factors
        Mesh mesh = ToOscMesh(model, state, inputMesh);
        auto vertices = mesh.vertices();
        auto compiled = warper.tryCreatePointWarper(document);
        compiled->warpInPlace(vertices);
        mesh.set_vertices(vertices);
        mesh.recalculate_normals();

        if (document.getShouldWriteWarpedMeshesToDisk()) {
            // the mesh should be written to disk in an appropriate mesh file format
            // and the resulting warped `OpenSim::Model` should link to the on-disk
            // data via an `OpenSim::Mesh`

            // figure out and prepare where the mesh data should be written
            const auto warpedMeshesDir = document.getWarpedMeshesOutputDirectory();
            OSC_ASSERT(warpedMeshesDir && "cannot figure out where to write warped mesh data: this will only work if the osim file was loaded from disk");
            const auto meshLocationAbsPath = std::filesystem::weakly_canonical(*warpedMeshesDir / GetMeshFileName(inputMesh));
            std::filesystem::create_directories(meshLocationAbsPath.parent_path());  // ensure parent directories are created

            // write mesh data to disk as a Wavefront OBJ file
            {
                std::ofstream objStream{meshLocationAbsPath, std::ios::trunc};
                objStream.exceptions(std::ios::badbit | std::ios::failbit);
                write_as_obj(objStream, mesh, ObjMetadata{"osc-model-warper"});
            }

            // return an `OpenSim::Mesh` thank refers to the OBJ file
            auto rv = std::make_unique<OpenSim::Mesh>();
            rv->set_mesh_file(meshLocationAbsPath.string());  // TODO: should be relative-ized, where reasonable
            return rv;
        }
        else {
            return std::make_unique<InMemoryMesh>(mesh);
        }
    }
}

class osc::mow::CachedModelWarper::Impl final {
public:
    std::shared_ptr<IModelStatePair> warp(const WarpableModel& document)
    {
        if (document != m_PreviousDocument) {
            m_PreviousResult = createWarpedModel(document);
            m_PreviousDocument = document;
        }
        return m_PreviousResult;
    }

    std::shared_ptr<IModelStatePair> createWarpedModel(const WarpableModel& document)
    {
        // copy the model into an editable "warped" version
        OpenSim::Model warpedModel{document.getModel()};
        InitializeModel(warpedModel);
        InitializeState(warpedModel);

        // iterate over each mesh in the model and warp it in-memory
        //
        // additionally, collect a base-frame-to-mesh lookup while doing this
        std::map<OpenSim::ComponentPath, std::vector<OpenSim::ComponentPath>> baseFrame2meshes;
        for (const auto& mesh : document.getModel().getComponentList<OpenSim::Mesh>()) {
            // try to warp+overwrite
            if (const auto* meshWarper = document.findMeshWarp(mesh)) {
                auto warpedMesh = WarpMesh(document, document.getModel(), document.getState(), mesh, *meshWarper);
                auto* targetMesh = FindComponentMut<OpenSim::Mesh>(warpedModel, mesh.getAbsolutePath());
                OSC_ASSERT_ALWAYS(targetMesh && "cannot find target mesh in output model: this should never happen");
                OverwriteGeometry(warpedModel, *targetMesh, std::move(warpedMesh));
            }
            else {
                return nullptr;  // no warper for the mesh (not even an identity warp): halt
            }

            // update base-frame-to-mesh lookup
            const auto& [it, inserted] = baseFrame2meshes.try_emplace(mesh.getFrame().findBaseFrame().getAbsolutePath());
            it->second.push_back(mesh.getAbsolutePath());
        }
        InitializeModel(warpedModel);
        InitializeState(warpedModel);

        // iterate over each `PathPoint` in the model (incl. muscle points) and warp them by
        // figuring out how each relates to a mesh in the model
        //
        // TODO: the `osc::mow::WarpableModel` should handle figuring out each point's warper, because
        // there are situations where there isn't a 1:1 relationship between meshes and bodies
        for (auto& pp : warpedModel.updComponentList<OpenSim::PathPoint>()) {
            auto baseFramePath = pp.getParentFrame().findBaseFrame().getAbsolutePath();
            if (auto it = baseFrame2meshes.find(baseFramePath); it != baseFrame2meshes.end()) {
                if (it->second.size() == 1) {
                    if (const auto* mesh = FindComponent<OpenSim::Mesh>(document.getModel(), it->second.front())) {
                        if (const auto meshWarper = document.findMeshWarp(*mesh)) {
                            // redefine the station's position in the mesh's coordinate system
                            auto posInMeshFrame = pp.getParentFrame().expressVectorInAnotherFrame(warpedModel.getWorkingState(), pp.get_location(), mesh->getFrame());
                            auto warpedInMeshFrame = to<SimTK::Vec3>(meshWarper->tryCreatePointWarper(document)->warp(to<Vec3>(posInMeshFrame)));
                            auto warpedInParentFrame = mesh->getFrame().expressVectorInAnotherFrame(warpedModel.getWorkingState(), warpedInMeshFrame, pp.getParentFrame());
                            pp.set_location(warpedInParentFrame);
                        }
                        else {
                            log_warn("no warper available for %s", it->second.front().toString().c_str());
                        }
                    }
                    else {
                        log_error("cannot find %s in the model: this shouldn't happen", it->second.front().toString().c_str());
                    }
                }
                else {
                    log_warn("cannot warp %s: there are multiple meshes attached to the same base frame, so it's ambiguous how to warp this point", pp.getName().c_str());
                }
            }
            else {
                log_warn("cannot warp %s: there don't appear to be any meshes attached to the same base frame?", pp.getName().c_str());
            }
        }

        for (auto& station : warpedModel.updComponentList<OpenSim::Station>()) {
            auto baseFramePath = station.getParentFrame().findBaseFrame().getAbsolutePath();
            if (auto it = baseFrame2meshes.find(baseFramePath); it != baseFrame2meshes.end()) {
                if (it->second.size() == 1) {
                    if (const auto* mesh = FindComponent<OpenSim::Mesh>(document.getModel(), it->second.front())) {
                        if (const auto meshWarper = document.findMeshWarp(*mesh)) {
                            // redefine the station's position in the mesh's coordinate system
                            auto posInMeshFrame = station.getParentFrame().expressVectorInAnotherFrame(warpedModel.getWorkingState(), station.get_location(), mesh->getFrame());
                            auto warpedInMeshFrame = to<SimTK::Vec3>(meshWarper->tryCreatePointWarper(document)->warp(to<Vec3>(posInMeshFrame)));
                            auto warpedInParentFrame = mesh->getFrame().expressVectorInAnotherFrame(warpedModel.getWorkingState(), warpedInMeshFrame, station.getParentFrame());
                            station.set_location(warpedInParentFrame);
                        }
                        else {
                            log_warn("no warper available for %s", it->second.front().toString().c_str());
                        }
                    }
                    else {
                        log_error("cannot find %s in the model: this shouldn't happen", it->second.front().toString().c_str());
                    }
                }
                else {
                    log_warn("cannot warp %s: there are multiple meshes attached to the same base frame, so it's ambiguous how to warp this point", station.getName().c_str());
                }
            }
            else {
                log_warn("cannot warp %s: there don't appear to be any meshes attached to the same base frame?", station.getName().c_str());
            }
        }

        InitializeModel(warpedModel);
        InitializeState(warpedModel);

        return std::make_shared<BasicModelStatePair>(
            warpedModel.getModel(),
            warpedModel.getWorkingState()
        );
    }
private:
    std::optional<WarpableModel> m_PreviousDocument;
    std::shared_ptr<IModelStatePair> m_PreviousResult;
};

osc::mow::CachedModelWarper::CachedModelWarper() :
    m_Impl{std::make_unique<Impl>()}
{}
osc::mow::CachedModelWarper::CachedModelWarper(CachedModelWarper&&) noexcept = default;
CachedModelWarper& osc::mow::CachedModelWarper::operator=(CachedModelWarper&&) noexcept = default;
osc::mow::CachedModelWarper::~CachedModelWarper() noexcept = default;

std::shared_ptr<IModelStatePair> osc::mow::CachedModelWarper::warp(const WarpableModel& document)
{
    return m_Impl->warp(document);
}
