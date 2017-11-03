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

#include "config.h"

#ifdef SYSTEMD

#include <systemd/sd-bus.h>

#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libudev.h>
#include <limits.h>

using std::cout;
using std::endl;

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
            controllers(new vector<VideoController>),
            videoOutputs(new vector<VideoOutput *>),
            videoOutputModes(new vector<VideoOutputModeInfo *>),
            availableControllers(new vector<VideoController>),
            monitors(new vector<Monitor *>) {

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
            VideoController *controller = (resources->crtcs + i);
            this->availableControllers->push_back(*controller);
            this->controllers->push_back(*controller);
        }

        for (i = 0; i < resources->noutput; i++) {
            VideoOutput *output = (resources->outputs + i);
            this->videoOutputs->push_back(output);
        }

        for (i = 0; i < resources->nmode; i++) {
            VideoOutputModeInfo *outputMode = (resources->modes + i);
            this->videoOutputModes->push_back(outputMode);
        }

    }

    DisplayManager::ScreenResources::~ScreenResources() {

        XRRFreeScreenResources(resources);

        delete videoOutputModes;
        delete controllers;
        delete videoOutputs;
        delete availableControllers;
        for (Monitor* monitor : *monitors) delete monitor;
        delete monitors;
    }

    vector<VideoController> *DisplayManager::ScreenResources::getControllers() const {
        return this->controllers;
    }

    vector<VideoOutput *> *DisplayManager::ScreenResources::getVideoOutputs() const {
        return this->videoOutputs;
    }

    vector<VideoOutputModeInfo *> *DisplayManager::ScreenResources::getVideoOutputModes() const {
        return this->videoOutputModes;
    }

    DisplayManager::XServer *DisplayManager::ScreenResources::getParentServer() const {
        return this->parentServer;
    }

    XRRScreenResources *DisplayManager::ScreenResources::getRawResources() {
        return this->resources;
    }

    void DisplayManager::ScreenResources::markControllerAsBusy(VideoController videoController) {

        int i = 0;

        for (VideoController controller : *availableControllers) {
            if (controller == videoController) {
                availableControllers->erase(availableControllers->begin() + i);
                break;
            }
            i++;
        }

    }

    void DisplayManager::ScreenResources::releaseController(VideoController videoController) {
        availableControllers->push_back(videoController);
    }

    VideoController DisplayManager::ScreenResources::requestController() {

        if (availableControllers->size() == 0) {
            return None;
        }

        VideoController ret = availableControllers->at(0);
        availableControllers->erase(availableControllers->begin());

        return ret;

    }

    vector<DisplayManager::Monitor *> *DisplayManager::ScreenResources::getMonitors() {

        // TODO: implement differential monitor detection

        if (this->monitors->size() != 0) {
            return this->monitors;
        }

        vector<VideoOutput *> *outputs = this->getVideoOutputs();

        for (VideoOutput *output : *outputs) {
            Monitor *monitor = new Monitor(output, this);
            this->monitors->push_back(monitor);
        }

        return this->monitors;

    }

    /******************** PowerManager ********************/

    bool PowerManagement::PowerStateManager::suspend() {

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

    bool PowerManagement::PowerStateManager::requestSuspend(SUSPEND_REASON reason) {

        Dock dock;

        switch (reason) {
            case SUSPEND_REASON_BUTTON:
                return PowerManagement::PowerStateManager::suspend();
            case SUSPEND_REASON_LID:
                if (!dock.probe()) {
                    fprintf(stderr, "dock is not sane/present");
                    return false;
                }

                if(!dock.isDocked()) {
                    PowerManagement::PowerStateManager::suspend();
                    return true;
                }

                fprintf(stderr, "ignoring lid event when docked\n");
                return false;
            default:
                fprintf(stderr, "invalid suspend reason \n");
                return false;
        }

    }


    DisplayManager::Monitor::Monitor(VideoOutput *videoOutput, ScreenResources *pResources) : videoOutput(videoOutput),
                                                                                              screenResources(
                                                                                                      pResources) {

        /* First, acquire a X server instance */

        XServer *server = XServer::getDefaultXServer();
        Display *display = server->getDisplay();
        XRRScreenResources *resources = screenResources->getRawResources();

        videoOutputInfo = XRRGetOutputInfo(display, resources, *videoOutput);

        if (!videoOutputInfo) {
            fprintf(stderr, "error fetching information about output (%lu)\n", *videoOutput);
            return;
        }

        if (videoOutputInfo->crtc == None) {
            return;
        }

        this->videoController = videoOutputInfo->crtc;
        screenResources->markControllerAsBusy(videoController);

        this->videoControllerInfo = XRRGetCrtcInfo(display, resources, this->videoController);

        if (!this->videoControllerInfo) {
            fprintf(stderr, "error fetching information from controller (%lu)\n", videoController);
            return;
        }

        if (videoControllerInfo->mode == None) {
            return;
        }

        vector<VideoOutputModeInfo *> *infos = screenResources->getVideoOutputModes();

        for (VideoOutputModeInfo *info : *infos) {
            if (info->id == videoControllerInfo->mode) {
                videoModeInfo = info;
            }
        }

        if (!videoModeInfo) {
            fprintf(stderr, "error fetching info about current mode\n");
        }
    }

    void DisplayManager::Monitor::turnOff() {

        if (videoControllerInfo == None) {
            return;
        }

        videoControllerInfo->mode = None;
        videoModeInfo = None;

    }

    bool DisplayManager::Monitor::isOff() const {

        if (videoController == None) {
            return true;
        }

        return videoControllerInfo->mode == None;

    }

    string DisplayManager::Monitor::getInterfaceName() const {
        return string(videoOutputInfo->name);
    }

    bool DisplayManager::Monitor::applyConfiguration() {

        XServer *server = screenResources->getParentServer();
        Display *display = screenResources->getParentServer()->getDisplay();
        Window window = screenResources->getParentServer()->getWindow();

        if (isOff()) {
            applyConfiguration(server, this);
            return true;
        }

        calculateLimits();
        calculateRelativePositions();

        XGrabServer(display);

        applyConfiguration(screenResources->getParentServer(), this);

        if (this->mirror != nullptr) {
            applyConfiguration(screenResources->getParentServer(), this->mirror);
        }

        if (isPrimary) {
            XRRSetOutputPrimary(display, window, *this->videoOutput);
        }

        Monitor *ptr = leftMonitor;
        while (ptr != nullptr) {
            applyConfiguration(server, ptr);
            ptr=ptr->leftMonitor;
        }

        ptr = rightMonitor;
        while (ptr != nullptr) {
            applyConfiguration(server, ptr);
            ptr=ptr->rightMonitor;
        }

        ptr = topMonitor;
        while (ptr != nullptr) {
            applyConfiguration(server, ptr);
            ptr=ptr->topMonitor;
        }

        ptr = bottomMonitor;
        while (ptr != nullptr) {
            applyConfiguration(server, ptr);
            ptr=ptr->bottomMonitor;
        }


#ifdef  DEBUG

        fprintf(stdout, "screen size: %dx%d (%dmmx%dmm)",
                (int) screenWidth,
                (int) screenHeight,
                (int) screenWidthMillimeters,
                (int) screenHeightMillimeters);

#endif

#ifndef DRYRUN

        XSync(display, False);

        fprintf(stdout, "setting screen size...\n");

            XRRSetScreenSize(display,
                             window,
                             (int) screenWidth,
                             (int) screenHeight,
                             (int) screenWidthMillimeters,
                             (int) screenHeightMillimeters);

#endif

        XUngrabServer(display);

        XSync(display, False);

        return true;

    }

    DisplayManager::point DisplayManager::Monitor::getPosition() const {

        if (videoControllerInfo == None) {
            fprintf(stderr, "requested position of inactive monitor\n");
            point pos;
            pos.x = -1;
            pos.y = -1;
            return pos;
        }

        point pos;

        pos.x = this->videoControllerInfo->x;
        pos.y = this->videoControllerInfo->y;

        return pos;

    }

    void DisplayManager::Monitor::setPosition(DisplayManager::point position) {

        if (videoControllerInfo == None) {
            fprintf(stderr, "requested to set position on inactive monitor\n");
            return;
        }

        this->videoControllerInfo->x = position.x;
        this->videoControllerInfo->y = position.y;

    }

    void DisplayManager::Monitor::setLeftMonitor(DisplayManager::Monitor *monitor) {
        this->leftMonitor = monitor;
    }

    void DisplayManager::Monitor::setRightMonitor(DisplayManager::Monitor *monitor) {
        this->rightMonitor = monitor;
    }

    void DisplayManager::Monitor::setTopMonitor(DisplayManager::Monitor *monitor) {
        this->topMonitor = monitor;
    }

    void DisplayManager::Monitor::setBottomMonitor(DisplayManager::Monitor *monitor) {
        this->bottomMonitor = monitor;
    }

    void DisplayManager::Monitor::setController(VideoController controller) {

        this->videoController = controller;
        this->videoControllerInfo = XRRGetCrtcInfo(XServer::getDefaultXServer()->getDisplay(),
                                                   screenResources->getRawResources(),
                                                   this->videoController);

        if (!this->videoControllerInfo) {
            fprintf(stderr, "error querying new controller\n");
        }

    }

    void DisplayManager::Monitor::setOutputMode(VideoOutputMode mode) {

        videoModeInfo = None;

        vector<VideoOutputModeInfo *> *infos = screenResources->getVideoOutputModes();

        for (VideoOutputModeInfo *info : *infos) {
            if (info->id == mode) {
                this->videoModeInfo = info;
                break;
            }
        }

        if (videoModeInfo == None) {
            fprintf(stderr, "error querying new output mode info\n");
        }

        this->videoControllerInfo->mode = mode;
        this->videoControllerInfo->height = videoModeInfo->height;
        this->videoControllerInfo->width = videoModeInfo->width;

    }

    bool DisplayManager::Monitor::isOutputModeSupported(VideoOutputMode mode) {

        for (int i = 0; i < this->videoOutputInfo->nmode; i++) {
            RRMode *rrMode = (this->videoOutputInfo->modes + i);
            if (*rrMode == mode) return true;
        }

        return false;

    }

    void DisplayManager::Monitor::release() {

        if (videoController != None) {
            screenResources->releaseController(videoController);
        }

        this->videoControllerInfo = None;
        this->videoController = None;
        this->videoModeInfo = None;

    }

    bool DisplayManager::Monitor::reconfigure() {

        VideoController controller = screenResources->requestController();

        if (controller == None) {
            fprintf(stderr, "can't reconfigure output: no available controllers");
            return false;
        }

        this->setController(controller);

        return true;
    }

    VideoOutputMode DisplayManager::Monitor::getPreferredOutputMode() const {
        int preferredMode = this->videoOutputInfo->npreferred - 1;
        return this->videoOutputInfo->modes[preferredMode];
    }

    bool DisplayManager::Monitor::isConnected() {
        return this->videoOutputInfo->connection == RR_Connected;
    }

    void DisplayManager::Monitor::calculateLimits() {

        xAxisMaxHeight = 0;
        yAxisMaxWidth = 0;

        xAxisMaxHeightmm = 0;
        yAxisMaxWidthmm = 0;

        screenWidth = 0;
        screenHeight = 0;

        screenWidthMillimeters = 0;
        screenHeightMillimeters = 0;

        /* first find the maximum height of monitors
         * along the X-axis
         *
         * start with the left wing
         */

        Monitor *ptr = leftMonitor;

        while (ptr != nullptr) {
            // pixels
            if (ptr->rotateNormalize(ptr->videoModeInfo->height) > xAxisMaxHeight) {
                xAxisMaxHeight = ptr->rotateNormalize(ptr->videoModeInfo->height);
            }
            // millimeters
            if (ptr->rotateNormalizeMillimeters(ptr->videoOutputInfo->mm_height) > xAxisMaxHeightmm) {
                xAxisMaxHeightmm = ptr->rotateNormalizeMillimeters(ptr->videoOutputInfo->mm_height);
            }

            screenWidth += ptr->rotateNormalize(ptr->videoModeInfo->width);
            screenWidthMillimeters += ptr->rotateNormalizeMillimeters(ptr->videoOutputInfo->mm_width);

            ptr = ptr->leftMonitor;
        }

        /* check out the central height */

        // pixels
        if (this->rotateNormalize(videoModeInfo->height) > this->xAxisMaxHeight) {
            this->xAxisMaxHeight = this->rotateNormalize(videoModeInfo->height);
        }
        // millimeters
        if (this->rotateNormalizeMillimeters(videoOutputInfo->mm_height) > xAxisMaxHeightmm) {
            this->xAxisMaxHeightmm = this->rotateNormalizeMillimeters(videoOutputInfo->mm_height);
        }

        screenWidth += this->rotateNormalize(videoModeInfo->width);
        screenWidthMillimeters += this->rotateNormalizeMillimeters(videoOutputInfo->mm_width);

        /* move to the right wing */

        ptr = rightMonitor;
        while (ptr != nullptr) {
            // pixels
            if (ptr->rotateNormalize(videoModeInfo->height) > this->xAxisMaxHeight) {
                this->xAxisMaxHeight = ptr->rotateNormalize(videoModeInfo->height);
            }
            // millimeters
            if (ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_height) > xAxisMaxHeightmm) {
                xAxisMaxHeightmm = ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_height);
            }

            screenWidth += ptr->rotateNormalize(videoModeInfo->width);
            screenWidthMillimeters += ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_width);

            ptr = ptr->rightMonitor;
        }

        /*
         * the maximum height over the X-Axis is now calculated, we now
         * move to the Y-axis
         */

        ptr = topMonitor;

        while (ptr != nullptr) {
            // pixels
            if (ptr->rotateNormalize(videoModeInfo->width) > this->yAxisMaxWidth) {
                this->yAxisMaxWidth = ptr->rotateNormalize(videoModeInfo->width);
            }
            // millimeter
            if (ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_width) > yAxisMaxWidthmm) {
                this->yAxisMaxWidthmm = ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_width);
            }

            screenHeight += ptr->rotateNormalize(videoModeInfo->height);
            screenHeightMillimeters += ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_height);

            ptr = ptr->topMonitor;
        }

        /* check out the central height */

        // pixels
        if (this->rotateNormalize(videoModeInfo->width) > this->yAxisMaxWidth) {
            this->yAxisMaxWidth = this->rotateNormalize(videoModeInfo->width);
        }

        // millimeter
        if (this->rotateNormalizeMillimeters(videoOutputInfo->mm_width) > yAxisMaxWidthmm) {
            this->yAxisMaxWidthmm = this->rotateNormalizeMillimeters(videoOutputInfo->mm_width);
        }

        screenHeight += this->rotateNormalize(videoModeInfo->height);
        screenHeightMillimeters += this->rotateNormalizeMillimeters(videoOutputInfo->mm_height);

        /*
         * move to the bottom wing and find the widest monitor
         */

        ptr = bottomMonitor;
        while (ptr != nullptr) {
            // pixels
            if (ptr->rotateNormalize(videoModeInfo->width) > this->yAxisMaxWidth) {
                this->yAxisMaxWidth = ptr->rotateNormalize(videoModeInfo->width);
            }
            // millimeter
            if (ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_width) > yAxisMaxWidthmm) {
                this->yAxisMaxWidthmm = ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_width);
            }

            screenHeight += ptr->rotateNormalize(videoModeInfo->height);
            screenHeightMillimeters += ptr->rotateNormalizeMillimeters(videoOutputInfo->mm_height);

            ptr = ptr->bottomMonitor;
        }

        /**
         * normalize the limits relative to maximums
         */


        screenWidth = yAxisMaxWidth > screenWidth ? yAxisMaxWidth : screenWidth;
        screenWidthMillimeters = yAxisMaxWidthmm > screenWidthMillimeters ? yAxisMaxWidthmm : screenWidthMillimeters;

        screenHeight = xAxisMaxHeight > screenHeight ? xAxisMaxHeight : screenHeight;
        screenHeightMillimeters = xAxisMaxHeightmm > screenHeightMillimeters ? xAxisMaxHeightmm : screenHeightMillimeters;


        /*
         * limit bounds have been calculated
         */

    }

    void DisplayManager::Monitor::calculateRelativePositions() {

        point root = this->getPrimaryRelativePosition();

        /* we now need to calculate the positions of the wings
         * (parent screens)
         *
         * screens positioned to the left have their Y point the same
         * as the primary screen but the X-point subtracted by their width
         *
         * traverse the left wing
         */

        point position = root;
        Monitor *ptr = leftMonitor;

        while (ptr != nullptr) {
            position.y = position.y;
            position.x = root.x - ptr->rotateNormalize(videoModeInfo->width);
            ptr->setPosition(position);
            ptr = ptr->leftMonitor;
        }

        /* left wing positions are now calculated for all monitor on the
         * left wing
         *
         * for the right wing, the start point for the Y axis is the same
         * but for the X-axis is root + primary width + next width
         */

        position = root;
        position.y = position.y;
        position.x = position.x + this->rotateNormalize(videoModeInfo->width);
        ptr = rightMonitor;

        while (ptr != nullptr) {
            ptr->setPosition(position);
            position.y = position.y;
            position.x = position.x + ptr->rotateNormalize(videoModeInfo->width);
            ptr = ptr->rightMonitor;
        }

        /*
         * all monitors on the x-axis are now aligned we now need
         * to align monitors on the Y-axis
         *
         * for top monitors, we start at the root of the primary
         * and add the monitor height
         */

        position = root;
        ptr = topMonitor;

        while (ptr != nullptr) {
            position.x = position.x;
            position.y = position.y - ptr->rotateNormalize(videoModeInfo->height);
            ptr->setPosition(position);
            ptr = ptr->topMonitor;
        }

        /*
         * the top wing is now aligned, move to the bottom wing.
         * the first screen of the bottom wing starts at root + primary height
         */

        position = root;
        position.x = position.x;
        position.y = position.y + this->rotateNormalize(videoModeInfo->height);
        ptr = bottomMonitor;

        while (ptr != nullptr) {
            ptr->setPosition(position);
            position.x = position.x;
            position.y = position.y + ptr->rotateNormalize(videoModeInfo->height);
            ptr = ptr->bottomMonitor;
        }

        /*
         * all monitors now have their positions calculated, ready
         * to be passed to the X server
         */


    }

    DisplayManager::point DisplayManager::Monitor::getPrimaryRelativePosition() {

         /* in order to calculate the root of the primary screen
         * (the top left corner) we need to calculate the width
         * of the left wing.
         */
        unsigned int leftWingWidth = 0;
        point root;

        Monitor *ptr = leftMonitor;

        while (ptr != nullptr) {
            leftWingWidth += ptr->rotateNormalize(videoModeInfo->width);
            ptr = ptr->leftMonitor;
        }

        /* we have the width of the left wing now, put it into the
         * point
         */

        root.x = leftWingWidth;

        /* now, we need the height of the top wing */

        unsigned int topWingTotal = 0;
        ptr = topMonitor;

        while (ptr != nullptr) {
            topWingTotal += ptr->rotateNormalize(videoModeInfo->height);
            ptr = ptr->topMonitor;
        }

        /* we have the height of the top wing, put it into the point */
        root.y = topWingTotal;

        this->setPosition(root);

        return root;

    }

    void
    DisplayManager::Monitor::applyConfiguration(DisplayManager::XServer *server, DisplayManager::Monitor *monitor) {

        Display *display = server->getDisplay();

#ifndef DRYRUN

        Status s = XRRSetCrtcConfig(display,
                                    monitor->screenResources->getRawResources(),
                                    monitor->videoController,
                                    CurrentTime,
                                    monitor->videoControllerInfo->x,
                                    monitor->videoControllerInfo->y,
                                    monitor->videoControllerInfo->mode,
                                    monitor->videoControllerInfo->rotation,
                                    monitor->videoControllerInfo->mode == None ? None : monitor->videoOutput,
                                    monitor->videoControllerInfo->mode == None ? 0 : 1);

        if (s != RRSetConfigSuccess) {
            fprintf(stderr, "error setting new screen config");
        }

#endif

    }

    void DisplayManager::Monitor::setPrimary(bool i) {
        this->isPrimary = i;
    }

    void DisplayManager::Monitor::setRotation(Rotation i) {
        this->videoControllerInfo->rotation = i;
    }

    unsigned int DisplayManager::Monitor::rotateNormalize(unsigned int unknownSize) {

        switch(this->videoControllerInfo->rotation) {
            case RR_Rotate_0:
            case RR_Rotate_180:

                return unknownSize;

            case RR_Rotate_90:
            case RR_Rotate_270:

                if (unknownSize == this->videoModeInfo->height)
                    return this->videoModeInfo->width;
                if (unknownSize == videoModeInfo->width)
                    return this->videoModeInfo->height;

                break;
        }

    }

    unsigned long DisplayManager::Monitor::rotateNormalizeMillimeters(unsigned long unknownSize) {

        switch(this->videoControllerInfo->rotation) {
            case RR_Rotate_0:
            case RR_Rotate_180:

                return unknownSize;

            case RR_Rotate_90:
            case RR_Rotate_270:

                if (unknownSize == this->videoOutputInfo->mm_height)
                    return this->videoOutputInfo->mm_width;
                if (unknownSize == videoOutputInfo->mm_width)
                    return this->videoOutputInfo->mm_height;

                break;
        }

    }

    void DisplayManager::Monitor::setMirror(DisplayManager::Monitor *pMonitor) {

        if (pMonitor == nullptr) {
            this->mirror = nullptr;
            return;
        }

        VideoOutputMode commonMode = this->findCommonOutputMode(this, pMonitor);

        this->setOutputMode(commonMode);
        pMonitor->setOutputMode(commonMode);

        pMonitor->setPosition(this->getPosition());

        this->mirror = pMonitor;

    }

    VideoOutputMode DisplayManager::Monitor::findCommonOutputMode(DisplayManager::Monitor *pMonitor,
                                                                   DisplayManager::Monitor *pMonitor1) {

        vector<VideoOutputModeInfo*> *modes = screenResources->getVideoOutputModes();

        for (VideoOutputModeInfo* info : *modes) {
            if (pMonitor->isOutputModeSupported(info->id) && pMonitor1->isOutputModeSupported(info->id)) {
                return info->id;
            }
        }

    }

    DisplayManager::Monitor::~Monitor() {

        if (this->videoControllerInfo != None) {
            XRRFreeCrtcInfo(this->videoControllerInfo);
        }

        if (this->videoOutputInfo != None) {
            XRRFreeOutputInfo(this->videoOutputInfo);
        }

    }

    /******************** ACPI ********************/

    pthread_t PowerManagement::ACPI::acpid_listener = -1;
    pthread_t PowerManagement::ACPI::udev_listener = -1;

    void *PowerManagement::ACPI::handle_acpid(void *_this) {

        ACPI *acpiClass = (ACPI*) _this;

        struct sockaddr_un addr;

        memset(&addr, 0, sizeof(struct sockaddr_un));

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, ACPID_SOCK, strlen(ACPID_SOCK));

        int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (connect(sfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_un)) < 0) {
            printf("Connect failed: %s\n", strerror(errno));
            return NULL;
        }

#ifdef DEBUG

        printf("Starting ACPI listener...\n");

#endif

        char buf[BUFSIZE];
        char inbuf[INBUFSZ];

        memset(buf, 0, BUFSIZE);
        memset(inbuf, 0, INBUFSZ);

        int numRead;
        int bufptr = 0;

        while ((numRead = read(sfd, inbuf, INBUFSZ)) > 0) {

            buf[bufptr] = inbuf[0];
            bufptr++;

            if (bufptr > BUFSIZE) {
                printf("Buffer full, purging event...\n");
                bufptr = 0;
                memset(buf, 0, BUFSIZE);
            }

            if (inbuf[0] == 0x0A) {

                ACPIEvent event = EVENT_UNKNOWN;

                if (strstr(buf, ACPI_POWERBUTTON) != NULL) {
                    event = EVENT_POWERBUTTON;
                }

                if (strstr(buf, ACPI_LID_OPEN) != NULL) {
                    event = EVENT_LID_OPEN;
                }

                if (strstr(buf, ACPI_LID_CLOSE) != NULL) {
                    event = EVENT_LID_CLOSE;
                }

                pthread_t handler;
                ACPIEventMetadata *metadata = (ACPIEventMetadata*) malloc(sizeof(ACPIEventMetadata));

                metadata->event = event;
                metadata->handler = acpiClass->ACPIhandler;

                pthread_create(&handler, NULL, ACPIEventHandler::_handleEvent, metadata);
                pthread_detach(handler);

                bufptr = 0;
                memset(buf, 0, BUFSIZE);
            }

        }

        close(sfd);

    }

    void* PowerManagement::ACPI::handle_udev(void* _this){

        ACPI *acpiClass = (ACPI*) _this;

        struct udev* udev = udev_new();
        struct udev_monitor *monitor = udev_monitor_new_from_netlink(udev, "udev");

        udev_monitor_filter_add_match_subsystem_devtype(monitor, "platform", NULL);
        udev_monitor_enable_receiving(monitor);

        int fd = udev_monitor_get_fd(monitor);

        fd_set set;
        struct timeval tv;

        tv.tv_sec = INT_MAX;
        tv.tv_usec = 0;

        struct udev_device *device;

        int ret;

        while (true) {

            FD_ZERO(&set);
            FD_SET(fd, &set);

            ret = select(fd + 1, &set, NULL, NULL, &tv);

            if (ret > 0 && FD_ISSET(fd, &set)) {

                device = udev_monitor_receive_device(monitor);

                if (device == NULL || strstr(udev_device_get_sysname(device), "dock.2") == NULL) {
                    printf("udev: device is invalid/not dock");
                    continue;
                }

                const char *docked = udev_device_get_sysattr_value(device, "docked");

                if (docked == NULL) {
                    printf("udev-fixme: udev docked device file invalid\n");
                    continue;
                }

                pthread_t handler;

                ACPIEventMetadata *metadata = (ACPIEventMetadata*) malloc(sizeof(ACPIEventMetadata));

                metadata->handler = acpiClass->ACPIhandler;
                metadata->event = *docked == '1' ? EVENT_DOCK : EVENT_UNDOCK;


                pthread_create(&handler, NULL, ACPIEventHandler::_handleEvent, metadata);
                pthread_detach(handler);

            }

        }

    }

    PowerManagement::ACPI::~ACPI()
    {
        if (acpid_listener > 0) {
            pthread_cancel(acpid_listener);
            pthread_join(acpid_listener, NULL);
        }

        if (udev_listener > 0) {
            pthread_cancel(udev_listener);
            pthread_join(udev_listener, NULL);
        }

    }

    void PowerManagement::ACPI::setEventHandler(PowerManagement::ACPIEventHandler *handler) {

        /* start the acpid event listener */
        this->ACPIhandler = handler;
        pthread_create(&acpid_listener, NULL, handle_acpid, this);

        /* start the udev event listener */
        pthread_create(&udev_listener, NULL, handle_udev, this);

    }

    void PowerManagement::ACPI::wait() {
        pthread_join(acpid_listener, NULL);
        pthread_join(udev_listener, NULL);
    }

    void *PowerManagement::ACPIEventHandler::_handleEvent(void* _this) {
        ACPIEventMetadata *metadata = (ACPIEventMetadata*) _this;
        metadata->handler->handleEvent(metadata->event);
        free(metadata);
    }

}


