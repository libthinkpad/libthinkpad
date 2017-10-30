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
#include <iostream>

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

    vector<DisplayManager::VideoOutput *>
    *DisplayManager::ScreenResources::getConnectedOutputs() {

        vector<VideoOutput*> *videoOutputs = this->getVideoOutputs();
        vector<VideoOutput*> *connectedOutputs = new vector<VideoOutput*>;

        for (VideoOutput *output : *videoOutputs) {
            if (output->isConnected()) {
                connectedOutputs->push_back(output);
            }
        }

        return connectedOutputs;

    }

    /******************** VideoController ********************/

    DisplayManager::VideoController::VideoController(VideoControllerType *id, ScreenResources *resources) :
            id(id),
            activeOutputs(new vector<VideoOutput*>),
            supportedOutputs(new vector<VideoOutput*>) {

        this->id = id;
        this->info = XRRGetCrtcInfo(resources->getParentServer()->getDisplay(), resources->getRawResources(), *id);
        if (!this->info) {
            fprintf(stderr, "Error reading infor for Video Controller!");
        }
        this->parent = resources;
    }

    DisplayManager::VideoController::~VideoController() {
        XRRFreeCrtcInfo(this->info);
        delete this->activeOutputs;
        delete this->supportedOutputs;
    }

    vector<DisplayManager::VideoOutput*>* DisplayManager::VideoController::getActiveOutputs() {

        this->activeOutputs->clear();
        vector<VideoOutput*> *allOutputs = parent->getVideoOutputs();

        for (int i = 0; i < info->noutput; i++) {
            RROutput *output = (info->outputs + i);
            for (VideoOutput *vo : *allOutputs) {
                if (*vo->getOutputId() == *output) {
                    this->activeOutputs->push_back(vo);
                }
            }
        }

        return this->activeOutputs;
    }

    VideoControllerType *DisplayManager::VideoController::getControllerId() const {
        return this->id;
    }

    int DisplayManager::VideoController::getXPosition() const {
        return info->x;
    }

    int DisplayManager::VideoController::getYPosition() const {
        return info->y;
    }

    void DisplayManager::VideoController::setPosition(point position) {
        this->info->x = position.x;
        this->info->y = position.y;
    }

    void DisplayManager::VideoController::addOutput(DisplayManager::VideoOutput *output) {

#ifdef DEBUG

        cout << "Configuring output " << *output->getName() << " (" << *output->getOutputId() << ") " << endl;

#endif

        RROutput *rrOutput = output->getOutputId();
        this->info->outputs = rrOutput;
        this->info->noutput = 1;
    }


    void DisplayManager::VideoController::setWidthPixels(unsigned int param) {
        this->info->width = param;
    }

    void DisplayManager::VideoController::seHeightPixels(unsigned int param) {
        this->info->height = param;
    }

    bool DisplayManager::VideoController::resetConfiguration() {

        /* output is already disabled */
        if (!this->isEnabled()) {
            return true;
        }

        this->info->noutput = 0;
        this->info->outputs = NULL;
        this->info->rotation = RR_Rotate_0;
        this->info->mode = None;
        this->info->x = 0;
        this->info->y = 0;

        return true;

    }

    void DisplayManager::VideoController::setOutputMode(DisplayManager::VideoOutputMode *pMode) {
        this->info->mode = pMode->getOutputModeId();
    }

    bool DisplayManager::VideoController::isEnabled() const {
        return this->info->mode != None;
    }

    vector<DisplayManager::VideoOutput *>* DisplayManager::VideoController::getSupportedOutputs() {

        this->supportedOutputs->clear();
        vector<VideoOutput*>* allOutputs = this->parent->getVideoOutputs();

        for (int i = 0; i < this->info->npossible; i++) {
            RROutput *possible = (this->info->possible + i);
            for (VideoOutput *output : *allOutputs) {
                if (*output->getOutputId() == *possible) {
                    this->supportedOutputs->push_back(output);
                }
            }
        }

        return this->supportedOutputs;

    }

    /******************** VideoOutput ********************/

    DisplayManager::VideoOutput::VideoOutput(VideoOutputType *type, ScreenResources *resources) :
            id(type) {

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

    VideoOutputType* DisplayManager::VideoOutput::getOutputId() const {
        return this->id;
    }

    bool DisplayManager::VideoOutput::isControllerSupported(DisplayManager::VideoController *pController) {
        vector<VideoOutput*>* supportedOutputs = pController->getSupportedOutputs();
        for (VideoOutput* output : *supportedOutputs) {
            if (output->getOutputId() == this->getOutputId()) {
                return true;
            }
        }

        return false;
    }

    void DisplayManager::VideoOutput::setController(DisplayManager::VideoController *pController) {
        this->info->crtc = *pController->getControllerId();
    }

    unsigned long DisplayManager::VideoOutput::getHeightMillimeters() const {
        return this->info->mm_height;
    }

    unsigned long DisplayManager::VideoOutput::getWidthMillimeters() const {
        return this->info->mm_width;
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

    unsigned int DisplayManager::VideoOutputMode::getWidthPixels() {
        return this->info->width;
    }

    unsigned int DisplayManager::VideoOutputMode::getHeightPixels() {
        return this->info->height;
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

    void DisplayManager::Monitor::setOutput(DisplayManager::VideoOutput* output) {
        this->videoOutput = output;
    }

    DisplayManager::VideoOutputMode *DisplayManager::Monitor::getPreferredOutputMode() const {
        return this->videoOutput->getPreferredOutputMode();
    }

    void DisplayManager::Monitor::setOutputMode(DisplayManager::VideoOutputMode *mode) {

        if (this->videoController == nullptr) {
            fprintf(stderr, "Before setting the output mode you must assign a controller to it\n");
            return;
        }

        this->videoMode = mode;
        this->videoController->setWidthPixels(videoMode->getWidthPixels());
        this->videoController->seHeightPixels(videoMode->getHeightPixels());
        this->videoController->setOutputMode(videoMode);

    }

    bool DisplayManager::Monitor::setController(DisplayManager::VideoController *pController) {
        if (!this->getOutput()->isControllerSupported(pController)) {
            fprintf(stderr, "controller not supported by output");
            return false;
        }
        this->videoController = pController;
        pController->addOutput(this->getOutput());
        this->videoOutput->setController(pController);
        return true;
    }

    DisplayManager::VideoOutput* DisplayManager::Monitor::getOutput() {
        return this->videoOutput;
    }


    bool DisplayManager::Monitor::isControllerSupported(DisplayManager::VideoController *pController) {
        return this->getOutput()->isControllerSupported(pController);
    }

     string* DisplayManager::Monitor::getName() {
        return this->getOutput()->getName();
    }

    void DisplayManager::Monitor::disable(ScreenResources *pResources) {
        this->videoController->resetConfiguration();

        Status s = XRRSetCrtcConfig(pResources->getParentServer()->getDisplay(),
                                    pResources->getRawResources(),
                                    *this->videoController->getControllerId(),
                                    CurrentTime,
                                    0,
                                    0,
                                    None,
                                    RR_Rotate_0,
                                    NULL,
                                    0);

        if (s != RRSetConfigSuccess) {
            fprintf(stderr, "error disabling display %s\n", this->getName()->c_str());
        }

    }

    void DisplayManager::Monitor::setRightWing(DisplayManager::Monitor *wing) {
        this->rightWing = wing;
    }

    void DisplayManager::Monitor::setLeftWing(DisplayManager::Monitor *wing) {
        this->leftWing = wing;
    }

    void DisplayManager::Monitor::setTopWing(DisplayManager::Monitor *wing) {
        this->topWing = wing;
    }

    void DisplayManager::Monitor::setBottomWing(DisplayManager::Monitor *wing) {
        this->bottomWing = wing;
    }

    unsigned int DisplayManager::Monitor::getTotalHeight() {

        /*
         * the maximum monitor width and height are unknown, calculate
         * those first
         */
        if (!this->limitsCalculated) {
            this->calculateLimits();
        }

        unsigned int height = 0;

        /* add the height of the top wing */

        Monitor *ptr = topWing;

        while (ptr != nullptr) {
            height += topWing->videoMode->getHeightPixels();
            ptr = ptr->topWing;
        }

        /* add the height of the central monitor */

        height += this->videoMode->getHeightPixels();

        /* add the height of the bottom wing */

        ptr = bottomWing;

        while (ptr != nullptr) {
            height += ptr->getTotalHeight();
            ptr = ptr->bottomWing;
        }

        /*
         * If the height of the X-Axis is higher than the
         * calculated size that means that some monitor along the X-Axis is
         * taller than the total height, thus we return that.
         */
        return xAxisMaxHeight > height ? xAxisMaxHeight : height;

    }

    unsigned int DisplayManager::Monitor::getTotalWidth() {

        /*
         * the maximum monitor width and height are unknown, calculate
         * those first
         */
        if (!this->limitsCalculated) {
            this->calculateLimits();
        }

        unsigned int width = 0;

        /* add the width up of the left wing */

        Monitor *ptr = leftWing;

        while (ptr != nullptr) {
            width += ptr->videoMode->getWidthPixels();
            ptr = ptr->leftWing;
        }

        /* central monitor reached, add it up */

        width += this->videoMode->getWidthPixels();

        /* add the width up of the right wing */

        ptr = rightWing;

        while (ptr != nullptr) {
            width += ptr->videoMode->getWidthPixels();
            ptr = ptr->rightWing;
        }

        /*
         * if we are aligning over the Y-Axis, instead of
         * returning the width added up return the maximum
         * monitor width over the Y-Axis
         */
        return yAxisMaxWidth > width ? yAxisMaxWidth : width;

    }

    void DisplayManager::Monitor::calculateLimits() {

        /* first find the maximum height of monitors
         * along the X-axis
         *
         * start with the left wing
         */

        Monitor *ptr = leftWing;

        while (ptr != nullptr) {
            // pixels
            if (ptr->videoMode->getHeightPixels() > xAxisMaxHeight) {
                xAxisMaxHeight = ptr->videoMode->getHeightPixels();
            }
            // millimeters
            if (ptr->videoOutput->getHeightMillimeters() > xAxisMaxHeightmm) {
                xAxisMaxHeightmm = ptr->videoOutput->getHeightMillimeters();
            }
            ptr = ptr->leftWing;
        }

        /* check out the central height */

        // pixels
        if (this->videoMode->getHeightPixels() > this->xAxisMaxHeight) {
            this->xAxisMaxHeight = this->videoMode->getHeightPixels();
        }
        // millimeters
        if (this->videoOutput->getHeightMillimeters() > xAxisMaxHeightmm) {
            this->xAxisMaxHeightmm = this->videoOutput->getHeightMillimeters();
        }

        /* move to the right wing */

        ptr = rightWing;
        while (ptr != nullptr) {
            // pixels
            if (ptr->videoMode->getHeightPixels() > this->xAxisMaxHeight) {
                this->xAxisMaxHeight = ptr->videoMode->getHeightPixels();
            }
            // millimeters
            if (ptr->videoOutput->getHeightMillimeters() > xAxisMaxHeightmm) {
                xAxisMaxHeightmm = ptr->videoOutput->getHeightMillimeters();
            }
            ptr = ptr->rightWing;
        }

        /*
         * the maximum height over the X-Axis is now calculated, we now
         * move to the Y-axis
         */

        ptr = topWing;

        while (ptr != nullptr) {
            // pixels
            if (ptr->videoMode->getWidthPixels() > this->yAxisMaxWidth) {
                this->yAxisMaxWidth = ptr->videoMode->getWidthPixels();
            }
            // millimeter
            if (ptr->videoOutput->getWidthMillimeters() > yAxisMaxWidthmm) {
                this->yAxisMaxWidthmm = ptr->videoMode->getWidthPixels();
            }
            ptr = ptr->topWing;
        }

        /* check out the central height */

        // pixels
        if (this->videoMode->getWidthPixels() > this->yAxisMaxWidth) {
            this->yAxisMaxWidth = this->videoMode->getWidthPixels();
        }

        // millimeter
        if (this->videoOutput->getWidthMillimeters() > yAxisMaxWidthmm) {
            this->yAxisMaxWidthmm = this->videoOutput->getWidthMillimeters();
        }

        /*
         * move to the bottom wing and find the widest monitor
         */

        ptr = bottomWing;
        while (ptr != nullptr) {
            // pixels
            if (ptr->videoMode->getWidthPixels() > this->yAxisMaxWidth) {
                this->yAxisMaxWidth = ptr->videoMode->getWidthPixels();
            }
            // millimeter
            if (ptr->videoOutput->getWidthMillimeters() > yAxisMaxWidthmm) {
                this->yAxisMaxWidthmm = ptr->videoMode->getWidthPixels();
            }
            ptr = ptr->bottomWing;
        }

        /*
         * limit bounds have been calculated
         */

        this->limitsCalculated = true;

    }

    DisplayManager::point DisplayManager::Monitor::getPrimaryPosition() {

        /* in order to calculate the root of the primary screen
         * (the top left corner) we need to calculate the width
         * of the left wing.
         */
        unsigned int leftWingWidth = 0;
        point root;

        Monitor *ptr = leftWing;

        while (ptr != nullptr) {
            leftWingWidth += ptr->videoMode->getWidthPixels();
            ptr = ptr->leftWing;
        }

        /* we have the width of the left wing now, put it into the
         * point
         */

        root.x = leftWingWidth;

        /* now, we need the height of the top wing */

        unsigned int topWingTotal = 0;
        ptr = topWing;

        while (ptr != nullptr) {
            topWingTotal += ptr->videoMode->getHeightPixels();
            ptr = ptr->topWing;
        }

        /* we have the height of the top wing, put it into the point */
        root.y = topWingTotal;

        this->videoController->setPosition(root);
        return root;

    }

    void DisplayManager::Monitor::calculateMonitorPositions() {

        point root = this->getPrimaryPosition();

        /* we now need to calculate the positions of the wings
         * (parent screens)
         *
         * screens positioned to the left have their Y point the same
         * as the primary screen but the X-point subtracted by their width
         *
         * traverse the left wing
         */

        point position = root;
        Monitor *ptr = leftWing;

        while (ptr != nullptr) {

            position.y = position.y;
            position.x = root.x - ptr->videoMode->getWidthPixels();

            ptr->videoController->setPosition(position);
            ptr = ptr->leftWing;
        }

        /* left wing positions are now calculated for all monitor on the
         * left wing
         *
         * for the right wing, the start point for the Y axis is the same
         * but for the X-axis is root + primary width + next width
         */

        position = root;

        position.y = position.y;
        position.x = position.x + this->videoMode->getWidthPixels();

        ptr = rightWing;

        while (ptr != nullptr) {

            ptr->videoController->setPosition(position);

            position.y = position.y;
            position.x = position.x + ptr->videoMode->getWidthPixels();

            ptr = ptr->leftWing;
        }

        /*
         * all monitors on the x-axis are now aligned we now need
         * to align monitors on the Y-axis
         *
         * for top monitors, we start at the root of the primary
         * and add the monitor height
         */

        position = root;
        ptr = topWing;

        while (ptr != nullptr) {

            position.x = position.x;
            position.y = position.y - ptr->videoMode->getHeightPixels();

            ptr->videoController->setPosition(position);
            ptr = ptr->topWing;
        }

        /*
         * the top wing is now aligned, move to the bottom wing.
         * the first screen of the bottom wing starts at root + primary height
         */

        position = root;

        position.x = position.x;
        position.y = position.y + this->videoMode->getHeightPixels();

        ptr = bottomWing;

        while (ptr != nullptr) {

            ptr->videoController->setPosition(position);

            position.x = position.x;
            position.y = position.y + ptr->videoMode->getHeightPixels();

            ptr = ptr->bottomWing;
        }

        /*
         * all monitors now have their positions calculated, ready
         * to be passed to the X server
         */

    }

    DisplayManager::dimensions DisplayManager::Monitor::getScreenDimensionsPixels() {
        dimensions dimens;
        dimens.width = this->getTotalWidth();
        dimens.height = this->getTotalHeight();
        return dimens;
    }

    DisplayManager::dimensions DisplayManager::Monitor::getScreenDimensionsMillimeters() {


        dimensions dimens;
        dimens.width = 0;
        dimens.height = 0;

        /* walk the left wing */

        Monitor *ptr = leftWing;
        while (ptr != nullptr) {
            dimens.width += ptr->videoOutput->getWidthMillimeters();
            ptr = ptr->leftWing;
        }

        /* center reached, add up the primary */
        dimens.width += this->videoOutput->getWidthMillimeters();

        /* walk the right wing */

        ptr = rightWing;
        while (ptr != nullptr) {
            dimens.width += ptr->videoOutput->getWidthMillimeters();
            ptr = ptr->leftWing;
        }

        /*
         * total width in mm calculated along the X-axis,
         * move to the Y-axis
         */

        ptr = topWing;
        while (ptr != nullptr) {
            dimens.height += ptr->videoOutput->getHeightMillimeters();
            ptr = ptr->topWing;
        }

        /* center reached, add up the primary */
        dimens.height += this->videoOutput->getHeightMillimeters();

        ptr = bottomWing;
        while (ptr != nullptr) {
            dimens.width += ptr->videoOutput->getHeightMillimeters();
            ptr = ptr->bottomWing;
        }

        /* readjust the maximums */

        dimens.width = dimens.width > yAxisMaxWidthmm ? dimens.width : yAxisMaxWidthmm;
        dimens.height = dimens.height > xAxisMaxHeightmm ? dimens.width : xAxisMaxHeightmm;

        return dimens;

    }

    void DisplayManager::Monitor::applyCascadingConfig(ScreenResources *pResources) {

        /* set the primary */

        setConfig(this, pResources);

        /* set the left wing */
        Monitor *ptr = leftWing;
        while (ptr != nullptr) {
            setConfig(ptr, pResources);
            ptr = ptr->leftWing;
        }

        /* set the left wing */
        ptr = rightWing;
        while (ptr != nullptr) {
            setConfig(ptr, pResources);
            ptr = ptr->rightWing;
        }

        /* set the left wing */
        ptr = bottomWing;
        while (ptr != nullptr) {
            setConfig(ptr, pResources);
            ptr = ptr->bottomWing;
        }

        /* set the left wing */
        ptr = topWing;
        while (ptr != nullptr) {
            setConfig(ptr, pResources);
            ptr = ptr->topWing;
        }

    }

    void
    DisplayManager::Monitor::setConfig(DisplayManager::Monitor *pMonitor, DisplayManager::ScreenResources *pResources) {

        VideoController *controller = pMonitor->videoController;
        VideoOutputMode *mode = pMonitor->videoMode;
        VideoOutput *output = pMonitor->videoOutput;

        XRRScreenResources *resources = pResources->getRawResources();
        Display *display = pResources->getParentServer()->getDisplay();

        Status s = XRRSetCrtcConfig(display,
                                    resources,
                                    *controller->getControllerId(),
                                    CurrentTime, controller->getXPosition(),
                                    controller->getYPosition(),
                                    mode->getOutputModeId(),
                                    RR_Rotate_0,
                                    output->getOutputId(),
                                    1);

        if (s != RRSetConfigSuccess) {
            fprintf(stderr, "config error!");
            return;
        }

    }


    /******************** MonitorManager ********************/

    vector<DisplayManager::Monitor *>
    *DisplayManager::ConfigurationManager::getAllMonitors() {

#ifdef DEBUG

        std::cout << "Listing all monitors..." << std::endl;

#endif

        this->allMonitors->clear();
        vector<VideoOutput*> *activeOutputs = this->resources->getConnectedOutputs();

        for (VideoOutput *output : *activeOutputs) {
            Monitor *monitor = new Monitor();
            monitor->setOutput(output);
            this->allMonitors->push_back(monitor);
        }

        activeOutputs->clear();
        delete activeOutputs;

        return this->allMonitors;
    }

    DisplayManager::ConfigurationManager::ConfigurationManager(DisplayManager::ScreenResources *resources) : allMonitors(new vector<Monitor*>) {

        this->resources = resources;
    }

    DisplayManager::ConfigurationManager::~ConfigurationManager() {
        for (Monitor *monitor : *allMonitors) delete monitor;
        delete allMonitors;
    }

    void DisplayManager::ConfigurationManager::commit() {


        if (this->primaryMonitor == nullptr) {
            fprintf(stderr, "primary monitor not specified\n");
            return;
        }

        XServer *server = this->resources->getParentServer();
        Display *display = server->getDisplay();

        /* start the config, grab the server */
        XGrabServer(display);

        this->primaryMonitor->calculateMonitorPositions();

        dimensions dimensPixels = primaryMonitor->getScreenDimensionsPixels();
        dimensions dimensmm = primaryMonitor->getScreenDimensionsMillimeters();

#ifdef DEBUG

        std::stringstream stream;

        stream << "width: ";
        stream << (int) dimensPixels.width;
        stream << endl;
        stream << "height: ";
        stream << (int) dimensPixels.height;
        stream << endl;
        stream << "width (mm): ";
        stream << (int) dimensmm.width;
        stream << endl;
        stream << "height (mm):";
        stream << (int) dimensmm.height;
        stream << endl;

        cout << stream.str();

#endif

        primaryMonitor->applyCascadingConfig(resources);

        XRRSetScreenSize(display,
                         server->getWindow(),
                         (int) dimensPixels.width,
                         (int) dimensPixels.height,
                         (int) dimensmm.width,
                         (int) dimensmm.height);


        /* end the config, ungrab the server */
        XUngrabServer(display);
    }


    void DisplayManager::ConfigurationManager::setMonitorPrimary(DisplayManager::Monitor *monitor) {
        this->primaryMonitor = monitor;
    }


}


