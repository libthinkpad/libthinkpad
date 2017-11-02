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

    Monitor *vga = nullptr;
    Monitor *lvds = nullptr;

    for (Monitor *monitor : *monitors) {

        if (monitor->isConnected() && monitor->getInterfaceName() == "VGA-1")
            vga = monitor;
        if (monitor->isConnected() && monitor->getInterfaceName() == "LVDS-1")
            lvds = monitor;
    }

    if (vga->isOff()) {
        vga->reconfigure();
    }

    if (lvds->isOff()) {
        lvds->reconfigure();
    }

    vga->setOutputMode(vga->getPreferredOutputMode());
    lvds->setOutputMode(lvds->getPreferredOutputMode());

    lvds->turnOff();
    lvds->applyConfiguration();
    lvds->release();

    vga->setPrimary(true);
    vga->applyConfiguration();


    return 0;

}