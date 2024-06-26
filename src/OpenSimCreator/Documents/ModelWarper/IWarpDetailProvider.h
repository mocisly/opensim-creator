#pragma once

#include <OpenSimCreator/Documents/ModelWarper/WarpDetail.h>

#include <vector>

namespace osc::mow
{
    // an interface to an object that can provide user-facing warp details at runtime
    class IWarpDetailProvider {
    protected:
        IWarpDetailProvider() = default;
        IWarpDetailProvider(const IWarpDetailProvider&) = default;
        IWarpDetailProvider(IWarpDetailProvider&&) noexcept = default;
        IWarpDetailProvider& operator=(const IWarpDetailProvider&) = default;
        IWarpDetailProvider& operator=(IWarpDetailProvider&&) noexcept = default;
    public:
        virtual ~IWarpDetailProvider() noexcept = default;
        std::vector<WarpDetail> details() const { return implWarpDetails(); }
    private:
        virtual std::vector<WarpDetail> implWarpDetails() const = 0;
    };
}
