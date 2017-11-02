#include "libthinkdock.h"
#include <iostream>
#include <cstring>

using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::endl;

using ThinkDock::DisplayManager::XServer;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::Monitor;
using ThinkDock::DisplayManager::point;
using ThinkDock::DisplayManager::dimensions;
using ThinkDock::PowerManager;

int main(void) {

    XServer *server = XServer::getDefaultXServer();
    ScreenResources *screenResources = new ScreenResources(server);
    vector<Monitor*> *monitors = screenResources->getMonitors();

    for (Monitor *monitor : *monitors) {

        if (monitor->isConnected() && monitor->getInterfaceName() == "LVDS-1")
            if (monitor->isOff()) {
                monitor->reconfigure();
                monitor->setOutputMode(monitor->getPreferredOutputMode());
                monitor->applyConfiguration();
            } else {
                monitor->turnOff();
                monitor->applyConfiguration();
                monitor->release();
            }


    }


    return 0;

}