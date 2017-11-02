#include "libthinkdock.h"
#include <iostream>
#include <cstring>
#include <zconf.h>

using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::endl;

using ThinkDock::DisplayManager::XServer;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::Monitor;
using ThinkDock::DisplayManager::point;
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

    printf("VGA < -- > LVDS\n");

    vga->setRotation(RR_Rotate_0);
    vga->setMirror(lvds);
    vga->setPrimary(true);
    vga->applyConfiguration();

    usleep(4000000);

    printf("Only VGA\n");

    lvds->turnOff();
    lvds->setRotation(RR_Rotate_0);
    lvds->applyConfiguration();
    vga->setMirror(nullptr);
    vga->setOutputMode(vga->getPreferredOutputMode());
    vga->applyConfiguration();

    usleep(4000000);

    printf("Only LVDS\n");

    vga->turnOff();
    vga->setPrimary(false);
    vga->applyConfiguration();
    lvds->setPrimary(true);
    lvds->setOutputMode(lvds->getPreferredOutputMode());
    lvds->setRotation(RR_Rotate_0);
    lvds->applyConfiguration();

    usleep(4000000);

    printf("VGA -> LVDS\n");

    vga->setOutputMode(vga->getPreferredOutputMode());
    lvds->setOutputMode(lvds->getPreferredOutputMode());
    vga->setPrimary(true);
    vga->setRightMonitor(lvds);
    vga->applyConfiguration();

    usleep(4000000);

    printf("VGA rotated 90\n");

    lvds->turnOff();
    lvds->applyConfiguration();
    vga->setRotation(RR_Rotate_90);
    vga->setPrimary(true);
    vga->setRightMonitor(nullptr);
    vga->applyConfiguration();

    usleep(4000000);


    printf("VGA > LVDS(90)\n");

    vga->setOutputMode(vga->getPreferredOutputMode());
    lvds->setOutputMode(lvds->getPreferredOutputMode());
    lvds->setRotation(RR_Rotate_90);
    vga->setRotation(RR_Rotate_0);
    vga->setRightMonitor(lvds);
    vga->setPrimary(true);
    vga->applyConfiguration();

    usleep(4000000);

    printf("Only VGA\n");

    lvds->turnOff();
    lvds->applyConfiguration();
    vga->setPrimary(true);
    vga->setRotation(RR_Rotate_0);
    vga->setOutputMode(vga->getPreferredOutputMode());
    vga->setRightMonitor(nullptr);
    vga->setMirror(nullptr);
    vga->applyConfiguration();

    delete screenResources;
    delete server;

    return 0;

}