#include "libthinkdock.h"
#include <memory>
#include <iostream>

using std::unique_ptr;
using std::shared_ptr;

using ThinkDock::DisplayManager::XServer;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::VideoController;
using ThinkDock::DisplayManager::VideoOutput;

int main(void) {

    unique_ptr<XServer> server(XServer::getDefaultXServer());
    unique_ptr<ScreenResources> resources(new ScreenResources(server.get()));

    ScreenResources *resources1 = resources.get();

    auto *controllers = resources1->getControllers();

    for (auto controller : *controllers) {

            unique_ptr<vector<shared_ptr<VideoOutput>>> activeOutputs(controller->getActiveOutputs());

            for (auto activeOutput : *activeOutputs) {

                auto preferredOutputType = activeOutput->getPreferredOutputMode();

                printf("output %s active on controller #%lu (%d,%d), preffered mode is %s @ %6.2fHz\n",
                       activeOutput->getName()->c_str(),
                       controller->getControllerId(),
                       controller->getXPosition(),
                       controller->getYPosition(),
                       preferredOutputType->getName()->c_str(),
                       preferredOutputType->getRefreshRate());

            }

    }

    return 0;

}