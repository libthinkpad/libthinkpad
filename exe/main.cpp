#include "libthinkdock.h"
#include <iostream>

using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::endl;

using ThinkDock::DisplayManager::XServer;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::VideoController;
using ThinkDock::DisplayManager::VideoOutput;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::Monitor;
using ThinkDock::DisplayManager::ConfigurationManager;
using ThinkDock::DisplayManager::point;
using ThinkDock::DisplayManager::dimensions;
using ThinkDock::PowerManager;

int main(void) {

    XServer *server = XServer::getDefaultXServer();
    ScreenResources *resources = new ScreenResources(server);
    ConfigurationManager *manager = new ConfigurationManager(resources);

    vector<VideoController*> *controllers = resources->getControllers();

    VideoController *firstController = controllers->at(0);
    VideoController *secondController = controllers->at(1);

    vector<Monitor*>* monitors = manager->getAllMonitors();

    Monitor *firstMonitor = monitors->at(0);
    Monitor *secondMonitor = monitors->at(1);

    if (firstMonitor->isControllerSupported(firstController)) {
        firstMonitor->setController(firstController);
    }

    if (secondMonitor->isControllerSupported(secondController)) {
        secondMonitor->setController(secondController);
    }

    secondMonitor->setOutputMode(secondMonitor->getPreferredOutputMode());
    firstMonitor->setOutputMode(firstMonitor->getPreferredOutputMode());

    manager->setMonitorPrimary(secondMonitor);
    firstMonitor->disable(resources);

    manager->commit();

    delete server;
    delete resources;
    delete manager;

    return 0;

}