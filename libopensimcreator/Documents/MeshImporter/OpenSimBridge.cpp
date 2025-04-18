#include "OpenSimBridge.h"

#include <libopensimcreator/ComponentRegistry/ComponentRegistry.h>
#include <libopensimcreator/ComponentRegistry/StaticComponentRegistries.h>
#include <libopensimcreator/Documents/MeshImporter/Body.h>
#include <libopensimcreator/Documents/MeshImporter/Document.h>
#include <libopensimcreator/Documents/MeshImporter/DocumentHelpers.h>
#include <libopensimcreator/Documents/MeshImporter/Joint.h>
#include <libopensimcreator/Documents/MeshImporter/Mesh.h>
#include <libopensimcreator/Documents/MeshImporter/MIIDs.h>
#include <libopensimcreator/Documents/MeshImporter/OpenSimExportFlags.h>
#include <libopensimcreator/Documents/MeshImporter/Station.h>
#include <libopensimcreator/Graphics/SimTKMeshLoader.h>
#include <libopensimcreator/Utils/OpenSimHelpers.h>
#include <libopensimcreator/Utils/SimTKConverters.h>

#include <liboscar/Graphics/Mesh.h>
#include <liboscar/Maths/Mat4.h>
#include <liboscar/Maths/MatFunctions.h>
#include <liboscar/Maths/MathHelpers.h>
#include <liboscar/Maths/Transform.h>
#include <liboscar/Maths/Vec3.h>
#include <liboscar/Platform/Log.h>
#include <liboscar/Utils/Algorithms.h>
#include <liboscar/Utils/Assertions.h>
#include <liboscar/Utils/UID.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentList.h>
#include <OpenSim/Simulation/Model/AbstractPathPoint.h>
#include <OpenSim/Simulation/Model/Frame.h>
#include <OpenSim/Simulation/Model/Geometry.h>
#include <OpenSim/Simulation/Model/Ground.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/OffsetFrame.h>
#include <OpenSim/Simulation/Model/PhysicalFrame.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>
#include <OpenSim/Simulation/Model/Station.h>
#include <OpenSim/Simulation/SimbodyEngine/Body.h>
#include <OpenSim/Simulation/SimbodyEngine/Coordinate.h>
#include <OpenSim/Simulation/SimbodyEngine/FreeJoint.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <OpenSim/Simulation/SimbodyEngine/PinJoint.h>
#include <OpenSim/Simulation/SimbodyEngine/WeldJoint.h>
#include <SimTKcommon.h>
#include <SimTKcommon/SmallMatrix.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

using namespace osc;
using namespace osc::mi;

namespace
{
    using Mesh = osc::mi::Mesh;

    // stand-in method that should be replaced by actual support for scale-less transforms
    // (dare i call them.... frames ;))
    Transform IgnoreScale(const Transform& t)
    {
        return t.with_scale(1.0f);
    }

    // attaches a mesh to a parent `OpenSim::PhysicalFrame` that is part of an `OpenSim::Model`
    void AttachMeshElToFrame(
        const Mesh& meshEl,
        const Transform& parentXform,
        OpenSim::PhysicalFrame& parentPhysFrame)
    {
        // create a POF that attaches to the body
        auto meshPhysOffsetFrame = std::make_unique<OpenSim::PhysicalOffsetFrame>();
        meshPhysOffsetFrame->setParentFrame(parentPhysFrame);
        meshPhysOffsetFrame->setName(std::string{meshEl.getLabel()} + "_offset");

        // set the POFs transform to be equivalent to the mesh's (in-ground) transform,
        // but in the parent frame
        const auto mesh2ground = to<SimTK::Transform>(meshEl.getXForm());
        const auto parent2ground = to<SimTK::Transform>(parentXform);
        meshPhysOffsetFrame->setOffsetTransform(parent2ground.invert() * mesh2ground);

        // attach the mesh data to the transformed POF
        auto mesh = std::make_unique<OpenSim::Mesh>(meshEl.getPath().string());
        mesh->setName(std::string{meshEl.getLabel()});
        mesh->set_scale_factors(to<SimTK::Vec3>(meshEl.getXForm().scale));
        AttachGeometry(*meshPhysOffsetFrame, std::move(mesh));

        // make it a child of the parent's physical frame
        AddComponent(parentPhysFrame, std::move(meshPhysOffsetFrame));
    }

    // create a body for the `model`, but don't add it to the model yet
    //
    // *may* add any attached meshes to the model, though
    std::unique_ptr<OpenSim::Body> CreateDetatchedBody(
        const Document& doc,
        const Body& bodyEl)
    {
        auto addedBody = std::make_unique<OpenSim::Body>();

        addedBody->setName(std::string{bodyEl.getLabel()});
        addedBody->setMass(bodyEl.getMass());

        // set the inertia of the emitted body to be nonzero
        //
        // the reason we do this is because having a zero inertia on a body can cause
        // the simulator to freak out in some scenarios.
        {
            const double moment = 0.01 * bodyEl.getMass();
            const SimTK::Vec3 moments{moment, moment, moment};
            const SimTK::Vec3 products{0.0, 0.0, 0.0};
            addedBody->setInertia(SimTK::Inertia{moments, products});
        }

        // connect meshes to the body, if necessary
        //
        // the body's orientation is going to be handled when the joints are added (by adding
        // relevant offset frames etc.)
        for (const osc::mi::Mesh& mesh : doc.iter<Mesh>())
        {
            if (mesh.getParentID() == bodyEl.getID())
            {
                AttachMeshElToFrame(mesh, bodyEl.getXForm(), *addedBody);
            }
        }

        return addedBody;
    }

    // result of a lookup for (effectively) a physicalframe
    struct JointAttachmentCachedLookupResult final {

        // can be nullptr (indicating Ground)
        const Body* bodyEl = nullptr;

        // can be nullptr (indicating ground/cache hit)
        std::unique_ptr<OpenSim::Body> createdBody;

        // always != nullptr, can point to `createdBody`, or an existing body from the cache, or Ground
        OpenSim::PhysicalFrame* physicalFrame = nullptr;
    };

    // cached lookup of a physical frame
    //
    // if the frame/body doesn't exist yet, constructs it
    JointAttachmentCachedLookupResult LookupPhysFrame(
        const Document& doc,
        OpenSim::Model& model,
        std::unordered_map<UID, OpenSim::Body*>& visitedBodies,
        UID elID)
    {
        // figure out what the parent body is. There's 3 possibilities:
        //
        // - null (ground)
        // - found, visited before (get it, but don't make it or add it to the model)
        // - found, not visited before (make it, add it to the model, cache it)

        JointAttachmentCachedLookupResult rv;
        rv.bodyEl = doc.tryGetByID<Body>(elID);
        rv.createdBody = nullptr;
        rv.physicalFrame = nullptr;

        if (rv.bodyEl)
        {
            const auto it = visitedBodies.find(elID);
            if (it == visitedBodies.end())
            {
                // haven't visited the body before
                rv.createdBody = CreateDetatchedBody(doc, *rv.bodyEl);
                rv.physicalFrame = rv.createdBody.get();

                // add it to the cache
                visitedBodies.emplace(elID, rv.createdBody.get());
            }
            else
            {
                // visited the body before, use cached result
                rv.createdBody = nullptr;  // it's not this function's responsibility to add it
                rv.physicalFrame = it->second;
            }
        }
        else
        {
            // the object is connected to ground
            rv.createdBody = nullptr;
            rv.physicalFrame = &model.updGround();
        }

        return rv;
    }

    // compute the name of a joint from its attached frames
    std::string CalcJointName(
        const Joint& jointEl,
        const OpenSim::PhysicalFrame& parentFrame,
        const OpenSim::PhysicalFrame& childFrame)
    {
        if (!jointEl.getUserAssignedName().empty())
        {
            return std::string{jointEl.getUserAssignedName()};
        }
        else
        {
            return childFrame.getName() + "_to_" + parentFrame.getName();
        }
    }

    // expresses if a joint has a degree of freedom (i.e. != -1) and the coordinate index of
    // that degree of freedom
    struct JointDegreesOfFreedom final {
        std::array<int, 3> orientation = {-1, -1, -1};
        std::array<int, 3> translation = {-1, -1, -1};
    };

    // returns the indices of each degree of freedom that the joint supports
    JointDegreesOfFreedom GetDegreesOfFreedom(const OpenSim::Joint& joint)
    {
        if (dynamic_cast<const OpenSim::FreeJoint*>(&joint))
        {
            return JointDegreesOfFreedom{{0, 1, 2}, {3, 4, 5}};
        }
        else if (dynamic_cast<const OpenSim::PinJoint*>(&joint))
        {
            return JointDegreesOfFreedom{{-1, -1, 0}, {-1, -1, -1}};
        }
        else
        {
            return JointDegreesOfFreedom{};  // unknown joint type
        }
    }

    // sets the names of a joint's coordinates
    void SetJointCoordinateNames(
        OpenSim::Joint& joint,
        const std::string& prefix)
    {
        constexpr auto c_TranslationNames = std::to_array({"_tx", "_ty", "_tz"});
        constexpr auto c_RotationNames = std::to_array({"_rx", "_ry", "_rz"});

        const JointDegreesOfFreedom dofs = GetDegreesOfFreedom(joint);

        // translations
        for (int i = 0; i < 3; ++i)
        {
            if (dofs.translation[i] != -1)
            {
                joint.upd_coordinates(dofs.translation[i]).setName(prefix + c_TranslationNames[i]);
            }
        }

        // rotations
        for (int i = 0; i < 3; ++i)
        {
            if (dofs.orientation[i] != -1)
            {
                joint.upd_coordinates(dofs.orientation[i]).setName(prefix + c_RotationNames[i]);
            }
        }
    }

    // recursively attaches `joint` to `model` by:
    //
    // - adding child bodies, if necessary
    // - adding an offset frames for each side of the joint
    // - computing relevant offset values for the offset frames, to ensure the bodies/joint-center end up in the right place
    // - setting the joint's default coordinate values based on any differences
    // - RECURSING by figuring out which joints have this joint's child as a parent
    void AttachJointRecursive(
        const Document& doc,
        OpenSim::Model& model,
        const Joint& joint,
        std::unordered_map<UID, OpenSim::Body*>& visitedBodies,
        std::unordered_set<UID>& visitedJoints)
    {
        {
            const bool wasInserted = visitedJoints.emplace(joint.getID()).second;
            if (!wasInserted)
            {
                // graph cycle detected: joint was already previously visited and shouldn't be traversed again
                return;
            }
        }

        // lookup each side of the joint, creating the bodies if necessary
        const JointAttachmentCachedLookupResult parent = LookupPhysFrame(doc, model, visitedBodies, joint.getParentID());
        JointAttachmentCachedLookupResult child = LookupPhysFrame(doc, model, visitedBodies, joint.getChildID());

        // create the parent OpenSim::PhysicalOffsetFrame
        auto parentPOF = std::make_unique<OpenSim::PhysicalOffsetFrame>();
        parentPOF->setName(parent.physicalFrame->getName() + "_offset");
        parentPOF->setParentFrame(*parent.physicalFrame);
        Mat4 toParentPofInParent =  inverse_mat4_cast(IgnoreScale(doc.getXFormByID(joint.getParentID()))) * mat4_cast(IgnoreScale(joint.getXForm()));
        parentPOF->set_translation(to<SimTK::Vec3>(Vec3{toParentPofInParent[3]}));
        parentPOF->set_orientation(to<SimTK::Vec3>(extract_eulers_xyz(toParentPofInParent)));

        // create the child OpenSim::PhysicalOffsetFrame
        auto childPOF = std::make_unique<OpenSim::PhysicalOffsetFrame>();
        childPOF->setName(child.physicalFrame->getName() + "_offset");
        childPOF->setParentFrame(*child.physicalFrame);
        const Mat4 toChildPofInChild = inverse_mat4_cast(IgnoreScale(doc.getXFormByID(joint.getChildID()))) * mat4_cast(IgnoreScale(joint.getXForm()));
        childPOF->set_translation(to<SimTK::Vec3>(Vec3{toChildPofInChild[3]}));
        childPOF->set_orientation(to<SimTK::Vec3>(extract_eulers_xyz(toChildPofInChild)));

        // create a relevant OpenSim::Joint (based on the type index, e.g. could be a FreeJoint)
        auto jointUniqPtr = Get(GetComponentRegistry<OpenSim::Joint>(), joint.getSpecificTypeName()).instantiate();

        // set its name
        const std::string jointName = CalcJointName(joint, *parent.physicalFrame, *child.physicalFrame);
        jointUniqPtr->setName(jointName);

        // set joint coordinate names
        SetJointCoordinateNames(*jointUniqPtr, jointName);

        // add + connect the joint to the POFs
        //
        // care: ownership change happens here (#642)
        OpenSim::PhysicalOffsetFrame& parentRef = AddFrame(*jointUniqPtr, std::move(parentPOF));
        const OpenSim::PhysicalOffsetFrame& childRef = AddFrame(*jointUniqPtr, std::move(childPOF));
        jointUniqPtr->connectSocket_parent_frame(parentRef);
        jointUniqPtr->connectSocket_child_frame(childRef);

        // if a child body was created during this step (e.g. because it's not a cyclic connection)
        // then add it to the model
        OSC_ASSERT_ALWAYS(parent.createdBody == nullptr && "at this point in the algorithm, all parents should have already been created");
        if (child.createdBody)
        {
            AddBody(model, std::move(child.createdBody));  // add created body to model
        }

        // add the joint to the model
        AddJoint(model, std::move(jointUniqPtr));

        // if there are any meshes attached to the joint, attach them to the parent
        for (const Mesh& mesh : doc.iter<Mesh>())
        {
            if (mesh.getParentID() == joint.getID())
            {
                AttachMeshElToFrame(mesh, joint.getXForm(), parentRef);
            }
        }

        // recurse by finding where the child of this joint is the parent of some other joint
        OSC_ASSERT_ALWAYS(child.bodyEl != nullptr && "child should always be an identifiable body object");
        for (const Joint& otherJoint : doc.iter<Joint>())
        {
            if (otherJoint.getParentID() == child.bodyEl->getID())
            {
                AttachJointRecursive(doc, model, otherJoint, visitedBodies, visitedJoints);
            }
        }
    }

    // attaches `BodyEl` into `model` by directly attaching it to ground with a WeldJoint
    void AttachBodyDirectlyToGround(
        const Document& doc,
        OpenSim::Model& model,
        const Body& bodyEl,
        std::unordered_map<UID, OpenSim::Body*>& visitedBodies)
    {
        std::unique_ptr<OpenSim::Body> addedBody = CreateDetatchedBody(doc, bodyEl);
        auto weldJoint = std::make_unique<OpenSim::WeldJoint>();
        auto parentFrame = std::make_unique<OpenSim::PhysicalOffsetFrame>();
        auto childFrame = std::make_unique<OpenSim::PhysicalOffsetFrame>();

        // set names
        weldJoint->setName(std::string{bodyEl.getLabel()} + "_to_ground");
        parentFrame->setName("ground_offset");
        childFrame->setName(std::string{bodyEl.getLabel()} + "_offset");

        // make the parent have the same position + rotation as the placed body
        parentFrame->setOffsetTransform(to<SimTK::Transform>(bodyEl.getXForm()));

        // attach the parent directly to ground and the child directly to the body
        // and make them the two attachments of the joint
        parentFrame->setParentFrame(model.getGround());
        childFrame->setParentFrame(*addedBody);
        weldJoint->connectSocket_parent_frame(*parentFrame);
        weldJoint->connectSocket_child_frame(*childFrame);

        // populate the "already visited bodies" cache
        visitedBodies[bodyEl.getID()] = addedBody.get();

        // add the components into the OpenSim::Model
        AddFrame(*weldJoint, std::move(parentFrame));
        AddFrame(*weldJoint, std::move(childFrame));
        AddBody(model, std::move(addedBody));
        AddJoint(model, std::move(weldJoint));
    }

    void AddStationToModel(
        const Document& doc,
        ModelCreationFlags flags,
        OpenSim::Model& model,
        const StationEl& stationEl,
        std::unordered_map<UID, OpenSim::Body*>& visitedBodies)
    {

        const JointAttachmentCachedLookupResult res = LookupPhysFrame(doc, model, visitedBodies, stationEl.getParentID());
        OSC_ASSERT_ALWAYS(res.physicalFrame != nullptr && "all physical frames should have been added by this point in the model-building process");

        const auto parentXform = to<SimTK::Transform>(doc.getByID(stationEl.getParentID()).getXForm(doc));
        const auto stationXform = to<SimTK::Transform>(stationEl.getXForm());
        const SimTK::Vec3 locationInParent = (parentXform.invert() * stationXform).p();

        if (flags & ModelCreationFlags::ExportStationsAsMarkers)
        {
            // export as markers in the model's markerset (overridden behavior)
            AddMarker(model, to_string(stationEl.getLabel()), *res.physicalFrame, locationInParent);
        }
        else
        {
            // export as stations in the given frame (default behavior)
            auto station = std::make_unique<OpenSim::Station>(*res.physicalFrame, locationInParent);
            station->setName(to_string(stationEl.getLabel()));

            AddComponent(*res.physicalFrame, std::move(station));
        }
    }

    // tries to find the first body connected to the given PhysicalFrame by assuming
    // that the frame is either already a body or is an offset to a body
    const OpenSim::PhysicalFrame* TryInclusiveRecurseToBodyOrGround(
        const OpenSim::Frame& f,
        std::unordered_set<const OpenSim::Frame*> visitedFrames)
    {
        if (!visitedFrames.emplace(&f).second)
        {
            return nullptr;
        }

        if (const auto* body = dynamic_cast<const OpenSim::Body*>(&f))
        {
            return body;
        }
        else if (const auto* ground = dynamic_cast<const OpenSim::Ground*>(&f))
        {
            return ground;
        }
        else if (const auto* pof = dynamic_cast<const OpenSim::PhysicalOffsetFrame*>(&f))
        {
            return TryInclusiveRecurseToBodyOrGround(pof->getParentFrame(), visitedFrames);
        }
        else if (const auto* station = dynamic_cast<const OpenSim::Station*>(&f))
        {
            return TryInclusiveRecurseToBodyOrGround(station->getParentFrame(), visitedFrames);
        }
        else
        {
            return nullptr;
        }
    }

    // tries to find the first body connected to the given PhysicalFrame by assuming
    // that the frame is either already a body or is an offset to a body
    const OpenSim::PhysicalFrame* TryInclusiveRecurseToBodyOrGround(const OpenSim::Frame& f)
    {
        return TryInclusiveRecurseToBodyOrGround(f, {});
    }

    Document CreateMeshImporterDocumentFromModel(std::unique_ptr<OpenSim::Model> ptr)
    {
        OpenSim::Model& m = *ptr;

        // init model+state
        InitializeModel(m);
        const SimTK::State& st = InitializeState(m);

        // this is what this method populates
        Document rv;

        // used to figure out how a body in the OpenSim::Model maps into the docuemnt
        std::unordered_map<const OpenSim::Body*, UID> bodyLookup;

        // used to figure out how a joint in the OpenSim::Model maps into the document
        std::unordered_map<const OpenSim::Joint*, UID> jointLookup;

        // import all the bodies from the model file
        for (const OpenSim::Body& b : m.getComponentList<OpenSim::Body>())
        {
            const std::string name = b.getName();
            const auto xform = to<Transform>(b.getTransformInGround(st));

            auto& el = rv.emplace<Body>(UID{}, name, xform);
            el.setMass(b.getMass());

            bodyLookup.emplace(&b, el.getID());
        }

        // then try and import all the joints (by looking at their connectivity)
        for (const OpenSim::Joint& j : m.getComponentList<OpenSim::Joint>())
        {
            const OpenSim::PhysicalFrame& parentFrame = j.getParentFrame();
            const OpenSim::PhysicalFrame& childFrame = j.getChildFrame();

            const OpenSim::PhysicalFrame* const parentBodyOrGround = TryInclusiveRecurseToBodyOrGround(parentFrame);
            const OpenSim::PhysicalFrame* const childBodyOrGround = TryInclusiveRecurseToBodyOrGround(childFrame);

            if (!parentBodyOrGround || !childBodyOrGround)
            {
                // can't find what they're connected to
                continue;
            }

            UID parent = MIIDs::Empty();
            if (dynamic_cast<const OpenSim::Ground*>(parentBodyOrGround))
            {
                parent = MIIDs::Ground();
            }
            else
            {
                if (const auto* body = lookup_or_nullptr(bodyLookup, dynamic_cast<const OpenSim::Body*>(parentBodyOrGround))) {
                    parent = *body;
                }
                else {
                    continue;  // joint is attached to a body that isn't ground or cached?
                }
            }

            UID child = MIIDs::Empty();
            if (dynamic_cast<const OpenSim::Ground*>(childBodyOrGround))
            {
                // ground can't be a child in a joint
                continue;
            }
            else
            {
                if (const auto* body = lookup_or_nullptr(bodyLookup, dynamic_cast<const OpenSim::Body*>(childBodyOrGround))) {
                    child = *body;
                }
                else {
                    continue;  // joint is attached to a body that isn't ground or cached?
                }
            }

            if (parent == MIIDs::Empty() || child == MIIDs::Empty())
            {
                // something horrible happened above
                continue;
            }

            const auto xform = to<Transform>(parentFrame.getTransformInGround(st));

            auto& jointEl = rv.emplace<Joint>(UID{}, j.getConcreteClassName(), j.getName(), parent, child, xform);
            jointLookup.emplace(&j, jointEl.getID());
        }


        // then try to import all the meshes
        for (const OpenSim::Mesh& mesh : m.getComponentList<OpenSim::Mesh>())
        {
            const std::optional<std::filesystem::path> maybeMeshPath = FindGeometryFileAbsPath(m, mesh);

            if (!maybeMeshPath)
            {
                continue;
            }

            const std::filesystem::path& realLocation = *maybeMeshPath;

            osc::Mesh meshData;
            try
            {
                meshData = LoadMeshViaSimTK(realLocation.string());
            }
            catch (const std::exception& ex)
            {
                log_error("error loading mesh: %s", ex.what());
                continue;
            }

            const OpenSim::Frame& frame = mesh.getFrame();
            const OpenSim::PhysicalFrame* const frameBodyOrGround = TryInclusiveRecurseToBodyOrGround(frame);

            if (!frameBodyOrGround)
            {
                // can't find what it's connected to?
                continue;
            }

            UID attachment = MIIDs::Empty();
            if (dynamic_cast<const OpenSim::Ground*>(frameBodyOrGround))
            {
                attachment = MIIDs::Ground();
            }
            else
            {
                if (const auto* body = lookup_or_nullptr(bodyLookup, dynamic_cast<const OpenSim::Body*>(frameBodyOrGround))) {
                    attachment = *body;
                }
                else {
                    continue;  // mesh is attached to something that isn't a ground or a body?
                }
            }

            if (attachment == MIIDs::Empty())
            {
                // couldn't figure out what to attach to
                continue;
            }

            auto& el = rv.emplace<Mesh>(UID{}, attachment, meshData, realLocation);
            auto newTransform = to<Transform>(frame.getTransformInGround(st));
            newTransform.scale = to<Vec3>(mesh.get_scale_factors());

            el.setXform(newTransform);
            el.setLabel(mesh.getName());
        }

        // then try to import all the stations
        for (const OpenSim::Station& station : m.getComponentList<OpenSim::Station>())
        {
            // edge-case: it's a path point: ignore it because it will spam the converter
            if (dynamic_cast<const OpenSim::AbstractPathPoint*>(&station))
            {
                continue;
            }

            if (OwnerIs<OpenSim::AbstractPathPoint>(station))
            {
                continue;
            }

            const OpenSim::PhysicalFrame& frame = station.getParentFrame();
            const OpenSim::PhysicalFrame* const frameBodyOrGround = TryInclusiveRecurseToBodyOrGround(frame);

            UID attachment = MIIDs::Empty();
            if (dynamic_cast<const OpenSim::Ground*>(frameBodyOrGround))
            {
                attachment = MIIDs::Ground();
            }
            else
            {
                if (const auto it = bodyLookup.find(dynamic_cast<const OpenSim::Body*>(frameBodyOrGround)); it != bodyLookup.end())
                {
                    attachment = it->second;
                }
                else
                {
                    // station is attached to something that isn't ground or a cached body
                    continue;
                }
            }

            if (attachment == MIIDs::Empty())
            {
                // can't figure out what to attach to
                continue;
            }

            const Vec3 pos = to<Vec3>(station.findLocationInFrame(st, m.getGround()));
            const std::string name = station.getName();

            rv.emplace<StationEl>(attachment, pos, name);
        }

        return rv;
    }
}

Document osc::mi::CreateModelFromOsimFile(const std::filesystem::path& p)
{
    return CreateMeshImporterDocumentFromModel(LoadModel(p));
}

std::unique_ptr<OpenSim::Model> osc::mi::CreateOpenSimModelFromMeshImporterDocument(
    const Document& doc,
    ModelCreationFlags flags,
    std::vector<std::string>& issuesOut)
{
    if (GetIssues(doc, issuesOut))
    {
        log_error("cannot create an osim model: issues detected");
        for (const std::string& issue : issuesOut)
        {
            log_error("issue: %s", issue.c_str());
        }
        return nullptr;
    }

    // create the output model
    auto model = std::make_unique<OpenSim::Model>();
    model->updDisplayHints().upd_show_frames() = true;

    // add any meshes that are directly connected to ground (i.e. meshes that are not attached to a body)
    for (const Mesh& meshEl : doc.iter<Mesh>())
    {
        if (meshEl.getParentID() == MIIDs::Ground())
        {
            AttachMeshElToFrame(meshEl, Transform{}, model->updGround());
        }
    }

    // keep track of any bodies/joints already visited (there might be cycles)
    std::unordered_map<UID, OpenSim::Body*> visitedBodies;
    std::unordered_set<UID> visitedJoints;

    // directly connect any bodies that participate in no joints into the model with a default joint
    for (const Body& bodyEl : doc.iter<Body>())
    {
        if (!IsAChildAttachmentInAnyJoint(doc, bodyEl))
        {
            AttachBodyDirectlyToGround(doc, *model, bodyEl, visitedBodies);
        }
    }

    // add bodies that do participate in joints into the model
    //
    // note: these bodies may use the non-participating bodies (above) as parents
    for (const Joint& jointEl : doc.iter<Joint>())
    {
        if (jointEl.getParentID() == MIIDs::Ground() || visitedBodies.contains(jointEl.getParentID()))
        {
            AttachJointRecursive(doc, *model, jointEl, visitedBodies, visitedJoints);
        }
    }

    // add stations into the model
    for (const StationEl& el : doc.iter<StationEl>())
    {
        AddStationToModel(doc, flags, *model, el, visitedBodies);
    }

    // invalidate all properties, so that model.finalizeFromProperties() *must*
    // reload everything with no caching
    //
    // otherwise, parts of the model (cough cough, OpenSim::Geometry::finalizeFromProperties)
    // will fail to load data because it will internally set itself as up to date, even though
    // it failed to load a mesh file because a parent was missing. See #330
    for (OpenSim::Component& c : model->updComponentList())
    {
        for (int i = 0; i < c.getNumProperties(); ++i)
        {
            c.updPropertyByIndex(i);
        }
    }

    // ensure returned model is initialized from latest document
    model->finalizeConnections();  // ensure all sockets are finalized to paths (#263)
    InitializeModel(*model);
    InitializeState(*model);

    return model;
}

Vec3 osc::mi::GetJointAxisLengths(const Joint& joint)
{
    const auto& registry = GetComponentRegistry<OpenSim::Joint>();

    JointDegreesOfFreedom dofs{};
    if (const auto idx = IndexOf(registry, joint.getSpecificTypeName())) {
        dofs = GetDegreesOfFreedom(registry[*idx].prototype());
    }

    Vec3 rv{};
    for (int i = 0; i < 3; ++i)
    {
        rv[i] = dofs.orientation[static_cast<size_t>(i)] == -1 ? 0.6f : 1.0f;
    }
    return rv;
}
