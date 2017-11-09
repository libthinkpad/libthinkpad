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

#include "libthinkpad.h"

#include "config.h"

#ifdef SYSTEMD

#include <systemd/sd-bus.h>

#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libudev.h>
#include <limits.h>
#include <cstring>
#include <math.h>

using std::cout;
using std::endl;

namespace ThinkPad {

    /******************** Dock ********************/

    bool Hardware::Dock::isDocked() {
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

    bool Hardware::Dock::probe() {
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

#ifndef THINKPAD_LEAN_AND_MEAN

    bool DisplayManagement::XServer::connect() {

        this->display = XOpenDisplay(NULL);

        if (!this->display) {
            fprintf(stderr, "Error opening X11 local connection");
            return false;
        }

        this->screen = DefaultScreen(display);
        this->window = RootWindow(display, screen);

        return true;

    }

    DisplayManagement::XServer *DisplayManagement::XServer::server;

    DisplayManagement::XServer *DisplayManagement::XServer::getDefaultXServer() {
        if (XServer::server == nullptr) {
            XServer::server = new XServer();
            if (!XServer::server->connect()) {
                fprintf(stderr, "Error connecting to default X Server!");
                return nullptr;
            }
        }
        return XServer::server;
    }

    DisplayManagement::XServer::~XServer() {
        XCloseDisplay(this->display);
    }

    Display *DisplayManagement::XServer::getDisplay() {
        return this->display;
    }

    Window DisplayManagement::XServer::getWindow() {
        return this->window;
    }

    /******************** ScreenResources ********************/

    DisplayManagement::ScreenResources::ScreenResources(XServer *server) :
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

    DisplayManagement::ScreenResources::~ScreenResources() {

        XRRFreeScreenResources(resources);

        delete videoOutputModes;
        delete controllers;
        delete videoOutputs;
        delete availableControllers;
        for (Monitor* monitor : *monitors) delete monitor;
        delete monitors;
    }

    vector<VideoController> *DisplayManagement::ScreenResources::getControllers() const {
        return this->controllers;
    }

    vector<VideoOutput *> *DisplayManagement::ScreenResources::getVideoOutputs() const {
        return this->videoOutputs;
    }

    vector<VideoOutputModeInfo *> *DisplayManagement::ScreenResources::getVideoOutputModes() const {
        return this->videoOutputModes;
    }

    DisplayManagement::XServer *DisplayManagement::ScreenResources::getParentServer() const {
        return this->parentServer;
    }

    XRRScreenResources *DisplayManagement::ScreenResources::getRawResources() {
        return this->resources;
    }

    void DisplayManagement::ScreenResources::markControllerAsBusy(VideoController videoController) {

        int i = 0;

        for (VideoController controller : *availableControllers) {
            if (controller == videoController) {
                availableControllers->erase(availableControllers->begin() + i);
                break;
            }
            i++;
        }

    }

    void DisplayManagement::ScreenResources::releaseController(VideoController videoController) {
        availableControllers->push_back(videoController);
    }

    VideoController DisplayManagement::ScreenResources::requestController() {

        if (availableControllers->size() == 0) {
            return None;
        }

        VideoController ret = availableControllers->at(0);
        availableControllers->erase(availableControllers->begin());

        return ret;

    }

    vector<DisplayManagement::Monitor *> *DisplayManagement::ScreenResources::getMonitors() {

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

#endif // THINKPAD_LEAN_AND_MEAN

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

    bool PowerManagement::PowerStateManager::requestSuspend(SuspendReason reason) {

        Hardware::Dock dock;

        switch (reason) {
            case SuspendReason::BUTTON:
                return PowerManagement::PowerStateManager::suspend();
            case SuspendReason::LID:
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

#ifndef THINKPAD_LEAN_AND_MEAN

    DisplayManagement::Monitor::Monitor(VideoOutput *videoOutput, ScreenResources *pResources) : videoOutput(videoOutput),
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

    void DisplayManagement::Monitor::turnOff() {

        if (videoControllerInfo == None) {
            return;
        }

        videoControllerInfo->mode = None;
        videoModeInfo = None;

    }

    bool DisplayManagement::Monitor::isOff() const {

        if (videoController == None) {
            return true;
        }

        return videoControllerInfo->mode == None;

    }

    string DisplayManagement::Monitor::getInterfaceName() const {
        return string(videoOutputInfo->name);
    }

    bool DisplayManagement::Monitor::applyConfiguration() {

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

    DisplayManagement::point DisplayManagement::Monitor::getPosition() const {

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

    void DisplayManagement::Monitor::setPosition(DisplayManagement::point position) {

        if (videoControllerInfo == None) {
            fprintf(stderr, "requested to set position on inactive monitor\n");
            return;
        }

        this->videoControllerInfo->x = position.x;
        this->videoControllerInfo->y = position.y;

    }

    void DisplayManagement::Monitor::setLeftMonitor(DisplayManagement::Monitor *monitor) {
        this->leftMonitor = monitor;
    }

    void DisplayManagement::Monitor::setRightMonitor(DisplayManagement::Monitor *monitor) {
        this->rightMonitor = monitor;
    }

    void DisplayManagement::Monitor::setTopMonitor(DisplayManagement::Monitor *monitor) {
        this->topMonitor = monitor;
    }

    void DisplayManagement::Monitor::setBottomMonitor(DisplayManagement::Monitor *monitor) {
        this->bottomMonitor = monitor;
    }

    void DisplayManagement::Monitor::setController(VideoController controller) {

        this->videoController = controller;
        this->videoControllerInfo = XRRGetCrtcInfo(XServer::getDefaultXServer()->getDisplay(),
                                                   screenResources->getRawResources(),
                                                   this->videoController);

        if (!this->videoControllerInfo) {
            fprintf(stderr, "error querying new controller\n");
        }

    }

    void DisplayManagement::Monitor::setOutputMode(VideoOutputMode mode) {

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

    bool DisplayManagement::Monitor::isOutputModeSupported(VideoOutputMode mode) {

        for (int i = 0; i < this->videoOutputInfo->nmode; i++) {
            RRMode *rrMode = (this->videoOutputInfo->modes + i);
            if (*rrMode == mode) return true;
        }

        return false;

    }

    void DisplayManagement::Monitor::release() {

        if (videoController != None) {
            screenResources->releaseController(videoController);
        }

        this->videoControllerInfo = None;
        this->videoController = None;
        this->videoModeInfo = None;

    }

    bool DisplayManagement::Monitor::reconfigure() {

        VideoController controller = screenResources->requestController();

        if (controller == None) {
            fprintf(stderr, "can't reconfigure output: no available controllers");
            return false;
        }

        this->setController(controller);

        return true;
    }

    VideoOutputMode DisplayManagement::Monitor::getPreferredOutputMode() const {
        int preferredMode = this->videoOutputInfo->npreferred - 1;
        return this->videoOutputInfo->modes[preferredMode];
    }

    bool DisplayManagement::Monitor::isConnected() {
        return this->videoOutputInfo->connection == RR_Connected;
    }

    void DisplayManagement::Monitor::calculateLimits() {

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

    void DisplayManagement::Monitor::calculateRelativePositions() {

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

    DisplayManagement::point DisplayManagement::Monitor::getPrimaryRelativePosition() {

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
    DisplayManagement::Monitor::applyConfiguration(DisplayManagement::XServer *server, DisplayManagement::Monitor *monitor) {

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

    void DisplayManagement::Monitor::setPrimary(bool i) {
        this->isPrimary = i;
    }

    void DisplayManagement::Monitor::setRotation(Rotation i) {
        this->videoControllerInfo->rotation = i;
    }

    unsigned int DisplayManagement::Monitor::rotateNormalize(unsigned int unknownSize) {

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

    unsigned long DisplayManagement::Monitor::rotateNormalizeMillimeters(unsigned long unknownSize) {

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

    void DisplayManagement::Monitor::setMirror(DisplayManagement::Monitor *pMonitor) {

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

    VideoOutputMode DisplayManagement::Monitor::findCommonOutputMode(DisplayManagement::Monitor *pMonitor,
                                                                   DisplayManagement::Monitor *pMonitor1) {

        vector<VideoOutputModeInfo*> *modes = screenResources->getVideoOutputModes();

        for (VideoOutputModeInfo* info : *modes) {
            if (pMonitor->isOutputModeSupported(info->id) && pMonitor1->isOutputModeSupported(info->id)) {
                return info->id;
            }
        }

    }

    DisplayManagement::Monitor::~Monitor() {

        if (this->videoControllerInfo != None) {
            XRRFreeCrtcInfo(this->videoControllerInfo);
        }

        if (this->videoOutputInfo != None) {
            XRRFreeOutputInfo(this->videoOutputInfo);
        }

    }

#endif // THINKPAD_LEAN_AND_MEAN

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

        printf("starting acpid listener...\n");

#endif

        char buf[BUFSIZE];
        char inbuf[INBUFSZ];

        memset(buf, 0, BUFSIZE);
        memset(inbuf, 0, INBUFSZ);

        int bufptr = 0;

        while (read(sfd, inbuf, INBUFSZ) > 0) {

            buf[bufptr] = inbuf[0];
            bufptr++;

            if (bufptr > BUFSIZE) {
                printf("Buffer full, purging event...\n");
                bufptr = 0;
                memset(buf, 0, BUFSIZE);
            }

            if (inbuf[0] == 0x0A) {

                ACPIEvent event = ACPIEvent::UNKNOWN;

                if (strstr(buf, ACPI_POWERBUTTON) != NULL) {
                    event = ACPIEvent::BUTTON_POWER;
                }

                if (strstr(buf, ACPI_LID_OPEN) != NULL) {
                    event = ACPIEvent::LID_OPENED;
                }

                if (strstr(buf, ACPI_LID_CLOSE) != NULL) {
                    event = ACPIEvent::LID_CLOSED;
                }

                if (strstr(buf, ACPI_BUTTON_MICMUTE) != NULL) {
                    event = ACPIEvent::BUTTON_MICMUTE;
                }

                if (strstr(buf, ACPI_BUTTON_MUTE) != NULL) {
                    event = ACPIEvent::BUTTON_MUTE;
                }

                if (strstr(buf, ACPI_BUTTON_THINKVANTAGE) != NULL) {
                    event = ACPIEvent::BUTTON_THINKVANTAGE;
                }

                if (strstr(buf, ACPI_BUTTON_FNF2_LOCK) != NULL) {
                    event = ACPIEvent::BUTTON_FNF2_LOCK;
                }

                if (strstr(buf, ACPI_BUTTON_FNF3_BATTERY) != NULL) {
                    event = ACPIEvent::BUTTON_FNF3_BATTERY;
                }

                if (strstr(buf, ACPI_BUTTON_FNF5_WLAN) != NULL) {
                    event = ACPIEvent::BUTTON_FNF5_WLAN;
                }

                if (strstr(buf, ACPI_BUTTON_FNF4_SLEEP) != NULL) {
                    event = ACPIEvent::BUTTON_FNF4_SLEEP;
                }

                if (strstr(buf, ACPI_BUTTON_FNF7_PROJECTOR) != NULL) {
                    event = ACPIEvent::BUTTON_FNF7_PROJECTOR;
                }

                if (strstr(buf, ACPI_BUTTON_FNF12_HIBERNATE) != NULL) {
                    event = ACPIEvent::BUTTON_FNF12_SUSPEND;
                }

                for (ACPIEventHandler* Acpihandler : *acpiClass->ACPIhandlers) {

                    pthread_t handler;
                    ACPIEventMetadata *metadata = (ACPIEventMetadata*) malloc(sizeof(ACPIEventMetadata));

                    metadata->event = event;
                    metadata->handler = Acpihandler;

                    pthread_create(&handler, NULL, ACPIEventHandler::_handleEvent, metadata);
                    pthread_detach(handler);

                }

                bufptr = 0;
                memset(buf, 0, BUFSIZE);
            }

        }

        close(sfd);

    }

    void* PowerManagement::ACPI::handle_udev(void* _this){

#ifdef DEBUG

        printf("starting udev listener...\n");

#endif

        ACPI *acpiClass = (ACPI*) _this;

        struct udev* udev = udev_new();
        struct udev_monitor *monitor = udev_monitor_new_from_netlink(udev, "udev");

        udev_monitor_filter_add_match_subsystem_devtype(monitor, "platform", NULL);
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "machinecheck", NULL);
        udev_monitor_enable_receiving(monitor);

        int fd = udev_monitor_get_fd(monitor);

        fd_set set;
        struct timeval tv;

        tv.tv_sec = INT_MAX;
        tv.tv_usec = 0;

        struct udev_device *device;

        int ret;

        ACPIEvent event = ACPIEvent::UNKNOWN;
        bool enteringS3S4 = false;

        while (acpiClass->udev_running) {

            FD_ZERO(&set);
            FD_SET(fd, &set);

            ret = select(fd + 1, &set, NULL, NULL, &tv);

            if (ret > 0 && FD_ISSET(fd, &set)) {

                device = udev_monitor_receive_device(monitor);

                if (device == NULL) {
#ifdef DEBUG
                    printf("udev: the device is null\n");
#endif
                    continue;
                }

                /*
                 * The /sys/devices/platform/dock.2 path is the main ThinkPad
                 * dock device file on XX20 series ThinkPads, other ThinkPads
                 * have not been tested as I don't have the hardware to test.
                 */
                if (strstr(udev_device_get_syspath(device), IBM_DOCK) != NULL) {

                    const char *docked = udev_device_get_sysattr_value(device, "docked");

                    if (docked == NULL) {
                        printf("udev-fixme: udev docked device file invalid\n");
                        continue;
                    }

                    event = *docked == '1' ? ACPIEvent::DOCKED : ACPIEvent::UNDOCKED;

                }

                /*
                 * When the system is suspending, Linux switches off all CPU cores
                 * but one, and this change is reflected in the sysfs with the
                 * removal/addition of the machinecheck files. We intercept these
                 * changes and act upon them
                 */
                if (strstr(udev_device_get_syspath(device), SYSFS_MACHINECHECK) != NULL) {

                    const char *action = udev_device_get_action(device);

                    if (strcmp(action, "remove") == 0) {

                        /**
                         * Each core except for CPU0 is brought down
                         * and then up again, we debounce this with
                         * only one event.
                         */
                        if (enteringS3S4) continue;

                        event = ACPIEvent::POWER_S3S4_ENTER;
                        enteringS3S4 = true;
                    }

                    if (strcmp(action, "add") == 0) {

                        if (!enteringS3S4) continue;

                        event = ACPIEvent::POWER_S3S4_EXIT;
                        enteringS3S4 = false;
                    }

                }

                for (ACPIEventHandler* acpihandler : *acpiClass->ACPIhandlers) {

                    pthread_t handler;

                    ACPIEventMetadata *metadata = (ACPIEventMetadata*) malloc(sizeof(ACPIEventMetadata));

                    metadata->handler = acpihandler;
                    metadata->event = event;


                    pthread_create(&handler, NULL, ACPIEventHandler::_handleEvent, metadata);
                    pthread_detach(handler);


                }

                event = ACPIEvent::UNKNOWN;

            }

        }

    }

    PowerManagement::ACPI::ACPI() : ACPIhandlers(new vector<ACPIEventHandler*>)
    {

    }

    PowerManagement::ACPI::~ACPI()
    {

        this->udev_running = false;

        if (acpid_listener > 0) {
            pthread_cancel(acpid_listener);
            pthread_join(acpid_listener, NULL);
        }

        if (udev_listener > 0) {
            pthread_cancel(udev_listener);
            pthread_join(udev_listener, NULL);
        }

        delete this->ACPIhandlers;

    }

    void PowerManagement::ACPI::addEventHandler(PowerManagement::ACPIEventHandler *handler) {
        this->ACPIhandlers->push_back(handler);
    }

    void PowerManagement::ACPI::wait() {
        pthread_join(acpid_listener, NULL);
        pthread_join(udev_listener, NULL);
    }

    void PowerManagement::ACPI::start()
    {
        /* start the acpid event listener */
        pthread_create(&acpid_listener, NULL, handle_acpid, this);

        /* start the udev event listener */
        pthread_create(&udev_listener, NULL, handle_udev, this);
    }

    void *PowerManagement::ACPIEventHandler::_handleEvent(void* _this) {
        ACPIEventMetadata *metadata = (ACPIEventMetadata*) _this;
        metadata->handler->handleEvent(metadata->event);
        free(metadata);
    }

    Configuration::~Configuration() {

        if (sections == nullptr) return;

        for (struct config_section_t* section : *sections) {
            for (struct config_keypair_t* pairs : *section->keypairs) {
                delete pairs;
            }
            delete section->keypairs;
            delete section;
        }

        delete sections;

    }

    vector<struct config_section_t*>* Configuration::parse(std::string path) {

        int fd = open(path.c_str(), O_RDONLY);

        if (fd < 0) {
            fprintf(stderr, "config: error opening: %s: %s", path.c_str(), strerror(errno));
            return nullptr;
        }

        struct stat buf;

        if (fstat(fd, &buf) < 0) {
            fprintf(stderr, "config: fstat failed: %s\n", strerror(errno));
            close(fd);
            return nullptr;
        }


        char file[buf.st_size];

        if (read(fd, file, buf.st_size) != buf.st_size) {
            fprintf(stderr, "config: read failed: %s\n", strerror(errno));
            close(fd);
            return nullptr;
        }

        close(fd);

        for(int i = 0; i < sizeof(file); i++) {

            continue_outer:

            /* skip leading whitespaces */
            while (file[i] == '\n') {
                i++;
                if (i >= sizeof(file))
                    goto break_outer;
            }

            if (file[i] != '[') {
                printf("config: unexpected token: %c\n", file[i]);
                break;
            }

            /* skip '[' */
            i++;
            if (i >= sizeof(file)) {
                printf("config: unexpected EOF\n");
                goto break_outer;
            }

            struct config_section_t *section = new struct config_section_t;
            section->keypairs = new vector<config_keypair_t*>;

            char *ptr = section->name;

            while (file[i] != ']') {
                *ptr = file[i];
                ptr++;
                i++;
                if (i >= sizeof(file)) {
                    printf("config: unclosed ]\n");
                    goto break_outer;
                }
            }

            /* skip ']' */
            i++;
            if (i >= sizeof(file)) {
                printf("config: unexpected EOF\n");
                goto break_outer;
            }

            while (true) {

                /* skip leading whitespaces */
                while (file[i] == '\n') {
                    i++;
                    if (i >= sizeof(file)) {
                        this->sections->push_back(section);
                        goto break_outer;
                    }
                }

                /* a new section is there */
                if (file[i] == '[') {
                    this->sections->push_back(section);
                    if (i >= sizeof(file))
                        goto break_outer;
                    goto continue_outer;
                }

                struct config_keypair_t* keypair = new struct config_keypair_t;
                ptr = keypair->key;

                while (file[i] != '=') {
                    *ptr = file[i];
                    ptr++;
                    i++;
                    if (i >= sizeof(file)) {
                        printf("config: unexpected EOF, expected '='\n");
                        goto break_outer;
                    }
                }

                /* skip '=' */
                i++;
                if (i >= sizeof(file)) {
                    printf("config: unexpected EOF, expected '='\n");
                    goto break_outer;
                }

                ptr = keypair->value;

                while (file[i] != '\n') {
                    *ptr = file[i];
                    ptr++;
                    i++;
                    if (i >= sizeof(file)) {
                        printf("config: unexpected EOF\n");
                        goto break_outer;
                    }
                }

                section->keypairs->push_back(keypair);

            }

        }

        break_outer:
        return this->sections;

    }

#define ASSERT_WRITE(write) if (write < 0) { printf("write failed: %s\n", strerror(errno)); return; }

    void Configuration::writeConfig(vector<config_section_t *> *sections, std::string path)
    {

        int fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);

        if (fd < 0) {
            printf("config: error writing config file: %s", strerror(errno));
            return;
        }

        for (struct config_section_t *section : *sections) {

            /* write the section name */
            ASSERT_WRITE(write(fd, "[", 1));
            ASSERT_WRITE(write(fd, section->name, strlen(section->name)));
            ASSERT_WRITE(write(fd, "]\n", 2));

            for (struct config_keypair_t* keypair : *section->keypairs) {

                ASSERT_WRITE(write(fd, keypair->key, strlen(keypair->key)));
                ASSERT_WRITE(write(fd, "=", 1));
                ASSERT_WRITE(write(fd, keypair->value, strlen(keypair->value)));
                ASSERT_WRITE(write(fd, "\n", 1));

            }

            ASSERT_WRITE(write(fd, "\n", 1));

        }

#ifdef DEBUG

        printf("config written to: %s\n", path.c_str());

#endif

        close(fd);

    }

    bool Hardware::ThinkLight::isOn()
    {
        int fd = open(SYSFS_THINKLIGHT, O_RDONLY);
        if (fd < 0) {
            printf("thinklight: invalid open: %s\n", strerror(errno));
            return false;
        }

        char buf[1];

        if (read(fd, buf, 1) != 1) {
            printf("thinklight: failed read: %s\n", strerror(errno));
            close(fd);
            return false;
        }

        close(fd);

        return *buf != '0';

    }

    bool Hardware::ThinkLight::probe()
    {
        int fd = open(SYSFS_THINKLIGHT, O_RDONLY);
        if (fd < 0) {
            close(fd);
            return false;
        }
        return true;
    }

    void Hardware::Backlight::setBacklightLevel(float factor) {

        bool intel = access(SYSFS_BACKLIGHT_INTEL, F_OK) == 0;
        bool nvidia = access(SYSFS_BACKLIGHT_NVIDIA, F_OK) == 0;

        if (intel) {
            float max = (float) getMaxBrightness(Backlight::System::INTEL);
            float setf = max * factor;
            setBrightness(Backlight::System::INTEL, (int) setf);
        }

        if (nvidia) {
            float max = (float) getMaxBrightness(Backlight::System::NVIDIA);
            float setf = max * factor;
            setBrightness(Backlight::System::NVIDIA, (int) setf);
        }

    }

    float Hardware::Backlight::getBacklightLevel() {

         // First try the Intel backlight system

        bool intel = access(SYSFS_BACKLIGHT_INTEL, F_OK) == 0;
        bool nvidia = access(SYSFS_BACKLIGHT_NVIDIA, F_OK) == 0;

        if (intel) {
            float max = (float) getMaxBrightness(Backlight::System::INTEL);
            float current = (float) getCurrentBrightness(Backlight::System::INTEL);
            return current / max;
        }

        if (nvidia) {
            float max = (float) getMaxBrightness(Backlight::System::NVIDIA);
            float current = (float) getCurrentBrightness(Backlight::System::NVIDIA);
            return current / max;
        }

    }

    void Hardware::Backlight::setBrightness(System system, int value) {

        int fd = -1;

        switch (system) {
            case Backlight::System ::NVIDIA:
                fd = open(SYSFS_BACKLIGHT_NVIDIA"/brightness", O_WRONLY);
                break;
            case Backlight::System ::INTEL:
                fd = open(SYSFS_BACKLIGHT_INTEL"/brightness", O_WRONLY);
                break;
        }

        if (fd < 0) {
            fprintf(stderr, "brightnes: error opening file for writing: %s\n", strerror(errno));
            return;
        }

        /**
         * The logarithm of a number with base 10 returns the
         * rough number of digits, we add +1 for the number of
         * digits, +1 for the null terminator
         * safety
         */
        size_t strlen = (size_t) log10f(value) + 2;

        char buf[strlen];
        memset(buf, 0, strlen);
        snprintf(buf, strlen, "%d", value);

        if (write(fd, buf, strlen) < 0) {
            fprintf(stderr, "brightnes: error writing to file: %s\n", strerror(errno));
        }

        close(fd);

    }

    int Hardware::Backlight::getMaxBrightness(Hardware::Backlight::System system) {

        int maxBrightness = -1;

        switch (system) {
            case Backlight::System::INTEL: {
                const char *data = fileRead(SYSFS_BACKLIGHT_INTEL"/max_brightness");
                maxBrightness = atoi(data);
                free((void *) data);
                break;
            }
            case Backlight::System::NVIDIA: {
                const char *data = fileRead(SYSFS_BACKLIGHT_INTEL"/max_brightness");
                maxBrightness = atoi(data);
                free((void *) data);
                break;
            }
        }

        if (maxBrightness < 0) {
            fprintf(stderr, "backlight: error reading backlight\n");
        }

        return maxBrightness;

    }

    int Hardware::Backlight::getCurrentBrightness(Hardware::Backlight::System system) {

        int brightness = -1;

        switch (system) {
            case Backlight::System::INTEL: {
                const char *data = fileRead(SYSFS_BACKLIGHT_INTEL"/brightness");
                brightness = atoi(data);
                free((void *) data);
                break;
            }
            case Backlight::System::NVIDIA: {
                const char *data = fileRead(SYSFS_BACKLIGHT_INTEL"/brightness");
                brightness = atoi(data);
                free((void *) data);
                break;
            }
        }

        if (brightness < 0) {
            fprintf(stderr, "backlight: error reading backlight\n");
        }

        return brightness;

    }

    const char *Hardware::Backlight::fileRead(const char *path) {

        int fd = open(path, O_RDONLY);

        if (fd < 0) {
            fprintf(stderr, "backlight: error opening max_brightness file: %s\n", strerror(errno));
            return NULL;
        }

        struct stat filestat;

        if (fstat(fd, &filestat) < 0) {
            fprintf(stderr, "backlight: error reading max_brightness filestats: %s\n", strerror(errno));
            close(fd);
            return NULL;
        }

        char *buf = (char*) calloc(sizeof(char), (size_t) filestat.st_size);

        if (read(fd, buf, (size_t) filestat.st_size) < 0) {
            fprintf(stderr, "backlight: error reading max_brightness: %s\n", strerror(errno));
        }

        close(fd);

        return buf;

    }
}


