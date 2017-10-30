/*
 * Copyright (c) 2017 Ognjen Galić
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include "libthinkdock.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace ThinkDock {

    /******************** Dock ********************/

    bool Dock::isDocked() {
        const int fd = open(IBM_DOCK_DOCKED, O_RDONLY);
        if (fd == ERR_INVALID) {
            close(fd);
            return false;
        }
        char status[1];
        ssize_t readBytes = read(fd, status, 1);
        if (readBytes == ERR_INVALID) {
            close(fd);
            return false;
        }
        close(fd);
        return status[0] == '1';
    }

    bool Dock::probe() {
        const int fd = open(IBM_DOCK_MODALIAS, O_RDONLY);
        if (fd == ERR_INVALID) {
            close(fd);
            return false;
        }
        struct stat buf;
        STATUS status = fstat(fd, &buf);
        if (status == ERR_INVALID) {
            close(fd);
            return false;
        }
        // allocate +1 bytes for the null terminator
        char readBuffer[buf.st_size + 1];
        ssize_t bytesRead = read(fd, readBuffer, (size_t) buf.st_size);
        readBuffer[buf.st_size] = 0;
        if (bytesRead == ERR_INVALID) {
            close(fd);
            return false;
        }
        return strcmp(readBuffer, IBM_DOCK_ID) == 0;
    }

    /******************** XServer ********************/

    bool DisplayManager::XServer::connect() {

        this->display = XOpenDisplay(NULL);

        if (!this->display) {
            fprintf(stderr, "Error opening X11 local connection");
            return false;
        }

        this->screen = DefaultScreen(display);
        this->window = RootWindow(display, screen);

        return true;

    }

    DisplayManager::XServer *DisplayManager::XServer::server;

    DisplayManager::XServer *DisplayManager::XServer::getDefaultXServer() {
        if (XServer::server == nullptr) {
            XServer::server = new XServer();
            if (!XServer::server->connect()) {
                fprintf(stderr, "Error connecting to default X Server!");
                return nullptr;
            }
        }
        return XServer::server;
    }

    DisplayManager::XServer::~XServer() {
        XCloseDisplay(this->display);
    }

    Display *DisplayManager::XServer::getDisplay() {
        return this->display;
    }

    Window DisplayManager::XServer::getWindow() {
        return this->window;
    }

    /******************** ScreenResources ********************/

    DisplayManager::ScreenResources::ScreenResources(XServer *server) :
            controllers(new vector<VideoController*>),
            videoOutputs(new vector<VideoOutput*>),
            videoOutputModes(new vector<VideoOutputMode*>) {

        Display *display = server->getDisplay();
        Window window = server->getWindow();
        this->resources = XRRGetScreenResourcesCurrent(display, window);
        this->parentServer = server;
        if (!this->resources) {
            fprintf(stderr, "Error getting screen resources!");
            return;
        }

        int i;

        for (i = 0; i < resources->ncrtc; i++) {
            VideoController *controller = new VideoController((resources->crtcs + i), this);
            this->controllers->push_back(controller);
        }

        for (i = 0; i < resources->noutput; i++) {
            VideoOutput *output = new VideoOutput((resources->outputs + i), this);
            this->videoOutputs->push_back(output);
        }

        for (i = 0; i < resources->nmode; i++) {
            VideoOutputMode *outputMode = new VideoOutputMode((resources->modes + i), this);
            this->videoOutputModes->push_back(outputMode);
        }

    }

    DisplayManager::ScreenResources::~ScreenResources() {
        XRRFreeScreenResources(resources);

        for (VideoOutput* videoOutput : *videoOutputs) delete videoOutput;
        for (VideoOutputMode *outputMode : *videoOutputModes) delete outputMode;
        for (VideoController *controller : *controllers) delete controller;

        this->videoOutputs->clear();
        this->controllers->clear();
        this->videoOutputModes->clear();

        delete videoOutputModes;
        delete controllers;
        delete videoOutputs;
    }

    XRRScreenResources *DisplayManager::ScreenResources::getScreenResources() const {
        return this->resources;
    }

    vector<DisplayManager::VideoController*> *DisplayManager::ScreenResources::getControllers() const {
        return this->controllers;
    }

    vector<DisplayManager::VideoOutput*> *DisplayManager::ScreenResources::getVideoOutputs() const {
        return this->videoOutputs;
    }

    vector<DisplayManager::VideoOutputMode*> *DisplayManager::ScreenResources::getVideoOutputModes() const {
        return this->videoOutputModes;
    }

    DisplayManager::XServer *DisplayManager::ScreenResources::getParentServer() const {
        return this->parentServer;
    }

    XRRScreenResources *DisplayManager::ScreenResources::getRawResources() {
        return this->resources;
    }

    vector<DisplayManager::VideoOutput*>
    *DisplayManager::ScreenResources::getConnectedOutputs(ScreenResources *resources) {

        vector<VideoOutput*> *videoOutputs = resources->getVideoOutputs();
        vector<VideoOutput*> *connectedOutputs = new vector<VideoOutput*>;

        for (VideoOutput *output : *videoOutputs) {
            if (output->isConnected()) {
                connectedOutputs->push_back(output);
            }
        }

        return connectedOutputs;

    }

    /******************** VideoController ********************/

    DisplayManager::VideoController::VideoController(VideoControllerType *id, ScreenResources *resources) {
        this->id = *id;
        this->info = XRRGetCrtcInfo(resources->getParentServer()->getDisplay(), resources->getRawResources(), *id);
        if (!this->info) {
            fprintf(stderr, "Error reading infor for Video Controller!");
        }
        this->parent = resources;
    }

    DisplayManager::VideoController::~VideoController() {
        XRRFreeCrtcInfo(this->info);
    }

    vector<DisplayManager::VideoOutput*>* DisplayManager::VideoController::getActiveOutputs() {

        vector<VideoOutput*> *allOutputs = parent->getVideoOutputs();
        vector<VideoOutput*> *active = new vector<VideoOutput*>;

        for (int i = 0; i < info->noutput; i++) {
            RROutput *output = (info->outputs + i);
            for (VideoOutput *vo : *allOutputs) {
                if (vo->getOutputId() == *output) {
                    active->push_back(vo);
                }
            }
        }

        return active;
    }

    VideoControllerType DisplayManager::VideoController::getControllerId() const {
        return this->id;
    }

    int DisplayManager::VideoController::getXPosition() const {
        return info->x;
    }

    int DisplayManager::VideoController::getYPosition() const {
        return info->y;
    }

    /******************** VideoOutput ********************/

    DisplayManager::VideoOutput::VideoOutput(VideoOutputType *type, ScreenResources *resources) :
            id(*type) {

        this->info = XRRGetOutputInfo(resources->getParentServer()->getDisplay(), resources->getRawResources(), *type);
        if (!this->info) {
            fprintf(stderr, "Error reading info for Video Output!");
        }
        this->name  = new string(this->info->name);
        this->parent = resources;
    }

    DisplayManager::VideoOutput::~VideoOutput() {
        XRRFreeOutputInfo(this->info);
        delete name;
    }


    DisplayManager::VideoOutputMode* DisplayManager::VideoOutput::getPreferredOutputMode() const {

        VideoOutputModeType *preferred = (this->info->modes + this->info->npreferred - 1);

        vector<VideoOutputMode*>* outputModes = this->parent->getVideoOutputModes();

        for (VideoOutputMode* outputMode : *outputModes) {
            if (*preferred == outputMode->getOutputModeId())
                return outputMode;
        }

    }

    bool DisplayManager::VideoOutput::isConnected() const {
        return info->connection == RR_Connected;
    }

    string *DisplayManager::VideoOutput::getName() const {
        return this->name;
    }

    VideoOutputType DisplayManager::VideoOutput::getOutputId() const {
        return this->id;
    }

    VideoOutputInfo *DisplayManager::VideoOutput::getOutputInfo() const {
        return this->info;
    }


    /******************** VideoOutputMode ********************/

    DisplayManager::VideoOutputMode::VideoOutputMode(XRRModeInfo *modeInfo, ScreenResources *resources) :
            info(modeInfo), name(new string(modeInfo->name)), id(modeInfo->id) {
        this->parent = resources;
    }

    string *DisplayManager::VideoOutputMode::getName() const {
        return this->name;
    }

    VideoOutputModeType DisplayManager::VideoOutputMode::getOutputModeId() const {
        return this->id;
    }

    double DisplayManager::VideoOutputMode::getRefreshRate() const {
        return ((double) info->dotClock) / ((double) info->hTotal * (double) info->vTotal);
    }

    string DisplayManager::VideoOutputMode::toString() const {

        std::ostringstream stream;
        stream << *this->getName();
        stream << " ";
        stream << this->getRefreshRate();
        stream << " (";
        stream << this->getOutputModeId();
        stream << ")";
        return stream.str();

    }

    DisplayManager::VideoOutputMode::~VideoOutputMode() {
        delete name;
    }

    /******************** PowerManager ********************/

    bool PowerManager::suspend() {

#ifdef SYSTEMD

        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = nullptr;
        int status;

        status = sd_bus_open_system(&bus);

        if (status < 0) {
            fprintf(stderr, "Connecting to D-Bus failed");
            return false;
        }

        status = sd_bus_call_method(bus,
                                    "org.freedesktop.login1",
                                    "/org/freedesktop/login1",
                                    "org.freedesktop.login1.Manager",
                                    "Suspend",
                                    &error,
                                    NULL,
                                    "b",
                                    "1");

        if (status < 0) {
            fprintf(stderr, "Error calling suspend on logind: %s\n", error.message);
            return false;
        }

        sd_bus_error_free(&error);
        sd_bus_unref(bus);

        return true;

#endif

        fprintf(stderr, "no suspend mechanism available\n");
        return false;

    }

    bool PowerManager::requestSuspend(SUSPEND_REASON reason) {

        Dock dock;

        switch (reason) {
            case SUSPEND_REASON_BUTTON:
                return PowerManager::suspend();
            case SUSPEND_REASON_LID:
                if (!dock.probe()) {
                    fprintf(stderr, "dock is not sane/present");
                    return false;
                }

                if(!dock.isDocked()) {
                    PowerManager::suspend();
                    return true;
                }

                fprintf(stderr, "ignoring lid event when docked\n");
                return false;
            default:
                fprintf(stderr, "invalid suspend reason \n");
                return false;
        }

    }

    /******************** Monitor ********************/

    void DisplayManager::Monitor::setPrimary() {
        // TODO: implement

        if (this->videoController == nullptr) {
            fprintf(stderr, "Before setting the output as primary you must assign a controller to it");
            return;
        }

        VideoOutputInfo* info = this->videoOutput->getOutputInfo();
    }

    void DisplayManager::Monitor::setOutput(DisplayManager::VideoOutput* output) {
        this->videoOutput = output;
    }

    DisplayManager::VideoOutputMode *DisplayManager::Monitor::getPreferredOutputMode() const {
        return this->videoOutput->getPreferredOutputMode();
    }

    /******************** MonitorManager ********************/

    vector<DisplayManager::Monitor*>
    *DisplayManager::MonitorManager::getAllMonitors(ScreenResources *screenResources) {

        vector<Monitor*> *monitors = new vector<Monitor*>;
        vector<VideoOutput*> *activeOutputs = screenResources->getConnectedOutputs(screenResources);

        for (VideoOutput *output : *activeOutputs) {
            Monitor *monitor = new Monitor();
            monitor->setOutput(output);
            monitors->push_back(monitor);
        }

        activeOutputs->clear();
        delete activeOutputs;

        return monitors;
    }
}


