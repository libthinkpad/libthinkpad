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

#ifndef LIBTHINKDOCK_LIBRARY_H
#define LIBTHINKDOCK_LIBRARY_H

#include "config.h"

#include <string>
#include <vector>
#include <memory>
#include <cstdio>

#include <X11/extensions/Xrandr.h>

#ifdef SYSTEMD
#include <systemd/sd-bus.h>
#endif

#define IBM_DOCK_DOCKED     "/sys/devices/platform/dock.2/docked"
#define IBM_DOCK_MODALIAS   "/sys/devices/platform/dock.2/modalias"
#define IBM_DOCK_ID         "acpi:IBM0079:PNP0C15:LNXDOCK:\n"
#define ERR_INVALID (-1)
#define STATUS int

using std::string;
using std::vector;
using std::shared_ptr;
using std::unique_ptr;

typedef RRMode VideoOutputModeType;
typedef RROutput VideoOutputType;
typedef RRCrtc VideoControllerType;
typedef XRROutputInfo VideoOutputInfo;
typedef XRRModeInfo VideoOutputModeInfo;
typedef XRRCrtcInfo VideoControllerInfo;

namespace ThinkDock {

    /**
     * The Dock class is used to probe for the dock
     * validity and probe for basic information about the dock.
     */
    class Dock {

    public:

        /**
         * Check if the ThinkPad is physically docked
         * into the UltraDock or the UltraBase
         * @return true if the ThinkPad is inside the dock
         */
        bool isDocked();

        /**
         * Probes the dock if it is an IBM dock and if the
         * dock is sane and ready for detection/state changes
         * @return true if the dock is sane and valid
         */
        bool probe();

    };

    class PowerManager {

    public:

        static bool suspend();

    };

    namespace DisplayManager {

        class XServer;
        class ScreenResources;
        class VideoOutput;
        class VideoController;
        class VideoOutputMode;

        /**
         * The VideoOutputMode class defines a mode for outputting
         * to a VideoOutput. All modes are common for all outputs, so
         * any mode can theoretically be set to any output.
         *
         * Defined parameters are resolution, refresh rate and output
         * mode (Interlanced/DoubleScan).
         */
        class VideoOutputMode {
        private:
            VideoOutputModeType id;
            VideoOutputModeInfo *info;
            ScreenResources *parent;
            unique_ptr<string> name;
        public:
            VideoOutputMode(XRRModeInfo *modeInfo, ScreenResources *resources);
            string *getName() const;
            VideoOutputModeType getOutputModeId() const;

            /**
             * Returns the ACTUAL refresh rate of the monitor, not
             * the doubled (DoubleScan) or halved (Interlanced)
             * refresh rate.
             * @return  the actual refresh rate of the monitor
             */
            double getRefreshRate() const;
        };

        /**
         * The VideoOutput class represents a physical output __PORT__,
         * NOT an actual device connected. The information about the
         * VideoOutput such as the height and width in millimeters, is fetched
         * for the current device CONNECTED to the port.
         */
        class VideoOutput {
        private:
            VideoOutputType id;
            VideoOutputInfo *info;
            ScreenResources *parent;
            unique_ptr<string> name;
        public:
            ~VideoOutput();
            VideoOutput(VideoOutputType *type, ScreenResources *resources);
            bool isConnected() const;

            /**
             * Return the visual representation of the output mode,
             * to be displayed on graphical interfaces.
             * For ex.: "640x480"
             * @return the textual representation of the output mode
             */
            string* getName() const;

            VideoOutputType getOutputId() const;

            /**
             * The preferred output mode is usually the monitors native
             * resolution and a refresh rate of 60Hz, even if monitors
             * support higher refresh rates.
             * @return the preffered video mode
             */
            shared_ptr<VideoOutputMode> getPreferredOutputMode() const;
        };

        /**
         * A graphics processing unit has so-called Video Display Controllers
         * (or VDC for short) that are image drivers for data from the GPU.
         * The VDC takes the data from the GPU and encodes it in an abstract format
         * that defines position, resolution and refresh rate.
         *
         * A GPU can drive only up to n-available VDCs, which means that a GPU
         * can't output more unique images than there are VDCs.
         *
         * However, a VDC can mirror the data to another output.
         *
         * For example, the Lenovo ThinkPad X220 has 2 VDC's and 8 outputs.
         * That means that the X220 can display up to 2 unqiue images that define
         * resolution, position and refresh rate, but those 2 images can be mirrored
         * and displayed on up-to 8 outputs.
         *
         * If you connect a projector to the VGA port and mirror the ThinkPad
         * screen to that, we are only using a single VDC to drive two outputs.
         * It is favorable for the outputs to share a common output mode that
         * both displays support, for ex. 800x600.
         *
         * The second VDC is not used and is disabled.
         *
         *           +-----------+     +-----------+
         *           |    VDC    |     |    VDC    |
         *           |  800x600  |     |    off    |
         *           |   59 Hz   |     |           |
         *           +-----+-----+     +-----------+
         *                 |
         *         +-------+-------+
         *         |               |
         *   +-----+-----+   +-----+-----+
         *   | ThinkPad  |   | Projector |
         *   |  Screen   |   |           |
         *   +-----------+   +-----------+
         *
         * However, if you connect a screen to the and "extend" the display area,
         * you are now using 2 VDC's and each VFC has its parameters defined, such
         * as resolution, position and refresh rate. The output modes can, but are
         * usually not common.
         *
         *          +-----------+     +-----------+
         *          |    VDC    |     |    VDC    |
         *          | 1366x768  |     | 1440x900  |
         *          |   59 Hz   |     |   75 Hz   |
         *          +-----+-----+     +-----+-----+
         *                |                 |
         *                |                 |
         *          +-----+-----+     +-----+-----+
         *          | ThinkPad  |     | External  |
         *          |  Screen   |     | Monitor   |
         *          +-----------+     +-----------+
         *
         * If running in "mirror" mode, each VDC has its position set to 0,0.
         * If running in "extend" mode, each VDC has its position set relative to the
         * primary monitor, which is considered the monitor positioned to 0,0.
         *
         */
        class VideoController {
        private:
            VideoControllerType id;
            VideoControllerInfo *info;
            ScreenResources *parent;
        public:
            ~VideoController();
            VideoController(VideoControllerType *id, ScreenResources *resources);

            /**
             * Get the currently active outputs on the video controller.
             * @return a list of active video outputs
             */
            vector<shared_ptr<VideoOutput>> *getActiveOutputs() const;
            VideoControllerType getControllerId() const;

            /*
             * The X and Y  positions determine relations between monitors.
             * The primary monitor is always located at 0.0.
             */

            /**
             * Gets the virtual Y position on the display pane.
             * @return the virtual Y position
             */
            int getXPosition() const;

            /**
             * Gets the virtual T position on the display pane.
             * @return the virtual Y position
             */
            int getYPosition() const;
        };

        /**
         * The ScreenResources class is actually the main container
         * of all the objects used by the display management system.
         * The ScreenResources class contains all the output modes, the
         * controllers and the physical outputs.
         */
        class ScreenResources {
        private:
            XRRScreenResources *resources;
            XServer *parentServer;
            unique_ptr<vector<shared_ptr<VideoController>>> controllers;
            unique_ptr<vector<shared_ptr<VideoOutput>>> videoOutputs;
            unique_ptr<vector<shared_ptr<VideoOutputMode>>> videoOutputModes;
        public:
            ScreenResources(XServer *server);
            ~ScreenResources();
            XRRScreenResources *getScreenResources() const;
            vector<shared_ptr<VideoController>> *getControllers() const;
            vector<shared_ptr<VideoOutput>> *getVideoOutputs() const;
            vector<shared_ptr<VideoOutputMode>> *getVideoOutputModes() const;
            XServer *getParentServer() const;
            XRRScreenResources *getRawResources();
        };

        /**
         * The XServer class is the main connection and relation
         * to the X server running on 0:0, localhost.
         */
        class XServer {
        private:
            Display *display;
            int screen;
            Window window;
            static XServer *server;
        public:
            ~XServer();
            bool connect();

            /**
             * The connection to the X server is a singleton
             * @return the singleton XServer instance
             */
            static XServer* getDefaultXServer();
            Display *getDisplay();
            Window getWindow();
        };


    };

};


#endif