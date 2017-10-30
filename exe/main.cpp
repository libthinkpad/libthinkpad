#include "libthinkdock.h"
#include <iostream>

using std::unique_ptr;
using std::shared_ptr;

using ThinkDock::DisplayManager::XServer;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::VideoController;
using ThinkDock::DisplayManager::VideoOutput;
using ThinkDock::DisplayManager::ScreenResources;
using ThinkDock::DisplayManager::Monitor;
using ThinkDock::DisplayManager::MonitorManager;
using ThinkDock::PowerManager;

int main(void) {

    XServer *server = XServer::getDefaultXServer();
    ScreenResources *resources = new ScreenResources(server);
    vector<VideoController*> *controllers = resources->getControllers();

    for (VideoController *controller : *controllers) controller->disableController(resources);

    MonitorManager *manager = new MonitorManager(resources);
    vector<Monitor*>* monitors = manager->getAllMonitors(resources);

    Monitor *first = monitors->at(0);
    VideoController *firstVideoController = controllers->at(0);

    if (first == nullptr) {
        fprintf(stderr, "Error finding first monitor\n");
        goto free;
    }

    if (!first->setController(firstVideoController)) {
        VideoController *second = controllers->at(1);
        if (!first->setController(second)) {
            goto free;
        }
    }

    first->setOutputMode(first->getPreferredOutputMode());
    first->applyConfiguration(resources);

    free:
    delete server;
    delete resources;
    delete manager;

    return 0;

}