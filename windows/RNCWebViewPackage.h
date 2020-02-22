// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "React/ICustomReactPackage.h"
#include "ReactSupport.h"

struct RNCWebViewPackage : ICustomReactPackage
{
    virtual void MakeModuleProviders(
        std::vector<facebook::react::NativeModuleDescription>& modules,
        const std::shared_ptr<facebook::react::MessageQueueThread>& /*defaultQueueThread*/) const override
    {
    }

    virtual void MakeViewProviders(
        std::vector<react::uwp::NativeViewManager>& /*viewManagers*/,
        const std::shared_ptr<react::uwp::IReactInstance>& /*instance*/) const override
    {
    }
};
