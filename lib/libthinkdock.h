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

#include <string>
#include <vector>
#include <memory>
#include <cstdio>

#include <X11/extensions/Xrandr.h>

#define IBM_DOCK_DOCKED     "/sys/devices/platform/dock.2/docked"
#define IBM_DOCK_MODALIAS   "/sys/devices/platform/dock.2/modalias"
#define IBM_DOCK_ID         "acpi:IBM0079:PNP0C15:LNXDOCK:\n"
#define ERR_INVALID (-1)

// #define DRYRUN

#define SUSPEND_REASON_LID 0
#define SUSPEND_REASON_BUTTON 1

using std::string;
using std::vector;

typedef RRMode VideoOutputMode;
typedef RROutput VideoOutput;
typedef RRCrtc VideoController;
typedef XRROutputInfo VideoOutputInfo;
typedef XRRModeInfo VideoOutputModeInfo;
typedef XRRCrtcInfo VideoControllerInfo;

typedef int SUSPEND_REASON;
typedef int STATUS;

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

    /**
     * The ThinkPad power manager. This handles the
     * system power state and ACPI event dispatches.
     */
    class PowerManager {

    private:

        /**
         * Calling this method suspends the ThinkPad without any
         * further questions
         * @return true if the suspend succeeded
         */
        static bool suspend();

    public:
        /**
         * Request a suspend of the system. You need to specify a suspend
         * reason, whether it is the lid or the user pressing the button.
         * This should be called in an ACPI event handler.
         * @param reason the reason for the suspend, lid or button
         * @return true if the suspend was successful
         */
        static bool requestSuspend(SUSPEND_REASON reason);
    };

    namespace DisplayManager {

        class XServer;
        class ScreenResources;
//        class VideoOutput;
//        class VideoController;
//        class VideoOutputMode;
        class Monitor;
//        class ConfigurationManager;
//        typedef struct _point point;
//        typedef struct _dimensions dimensions;
//
//        /**
//         * The VideoOutputMode class defines a mode for outputting
//         * to a VideoOutput. All modes are common for all outputs, so
//         * any mode can theoretically be set to any output.
//         *
//         * Defined parameters are resolution, refresh rate and output
//         * mode (Interlanced/DoubleScan).
//         */
//        class VideoOutputMode {
//
//        private:
//
//            VideoOutputModeType id;
//            VideoOutputModeInfo *info;
//            ScreenResources *parent;
//            string *name;
//            Rotation rotation = RR_Rotate_0;
//
//        public:
//
//            VideoOutputMode(XRRModeInfo *modeInfo, ScreenResources *resources);
//            ~VideoOutputMode();
//
//            /**
//             * The string representation of the output mode,
//             * for example "640x480"
//             *
//             * @return the visual string representation of the mode
//             */
//            string *getName() const;
//
//            /**
//             * Returns the output mode ID used by the X11 RandR
//             * extension for internal operations.
//             *
//             * @return the ID of the mode for X11
//             */
//            VideoOutputModeType getOutputModeId() const;
//
//            /**
//             * Returns the ACTUAL refresh rate of the monitor, not
//             * the doubled (DoubleScan) or halved (Interlanced)
//             * refresh rate.
//             *
//             * @return  the actual refresh rate of the monitor
//             */
//            double getRefreshRate() const;
//
//            /**
//             * Returns a debug-format string of the output mode.
//             * Not to be used by the user, used for debugging
//             *
//             * @return a detailed representation of the output mode
//             */
//            string toString() const;
//
//            /**
//             * Returns the width of the output mode in pixels
//             *
//             * @return the width in pixels
//             */
//            unsigned int getWidthPixels();
//
//            /**
//             * Returns the height of the output mode in pixels
//             *
//             * @return the height in pixels
//             */
//            unsigned int getHeightPixels();
//
//            void setRotation(Rotation i);
//
//
//        };
//
//        /**
//         * The VideoOutput class represents a physical output __PORT__,
//         * NOT an actual device connected. The information about the
//         * VideoOutput such as the height and width in millimeters, is fetched
//         * for the current device CONNECTED to the port.
//         */
//        class VideoOutput {
//
//        private:
//
//            VideoOutputType *id;
//            VideoOutputInfo *info;
//            ScreenResources *parent;
//            string *name;
//            Rotation rotation = RR_Rotate_0;
//
//        public:
//            ~VideoOutput();
//            VideoOutput(VideoOutputType *type, ScreenResources *resources);
//
//            /**
//             * Checks if the video output is physically connected to the
//             * output device
//             * @return true if the output is connected
//             */
//            bool isConnected() const;
//
//            /**
//             * Return the visual representation of the output mode,
//             * to be displayed on graphical interfaces.
//             * For ex.: "LVDS-1" or "HDMI-1"
//             * @return the textual representation of the output mode
//             */
//            string* getName() const;
//
//            /**
//             * This returns the output ID (RROutput) used by the X11
//             * RandR extensions
//             * @return the RROutput value
//             */
//            VideoOutputType *getOutputId() const;
//
//            /**
//             * The preferred output mode is usually the monitors native
//             * resolution and a refresh rate of 60Hz, even if monitors
//             * support higher refresh rates.
//             * @return the preffered video mode
//             */
//            VideoOutputMode* getPreferredOutputMode() const;
//
//            /**
//             * Returns true if the controller can be used to output on this
//             * output
//             * @param pController the controller to be tested
//             * @return true if the controller is supported
//             */
//            bool isControllerSupported(VideoController *pController);
//
//            /**
//             * Sets the output controller to the output.
//             * @param pController
//             */
//            void setController(VideoController *pController);
//
//            /**
//             * Gets the output device screen area width in millimeters. The data
//             * is fetched from the EDID of the output device.
//             * @return the width in millimeters of the output device
//             */
//            unsigned long getWidthMillimeters() const;
//
//            /**
//             * Gets the output device screen area width in millimeters. The data
//             * is fetched from the EDID of the output device.
//             * @return the height in millimeters of the output device
//             */
//            unsigned long getHeightMillimeters() const;
//
//            /**
//             * Check if an output mode is supported by this output device
//             * @param pMode the mode to test if it is supported
//             * @return true if the mode is supported
//             */
//            bool isOutputModeSupported(VideoOutputMode *pMode);
//
//            void setRotation(Rotation i);
//
//
//            RRCrtc getControllerId();
//        };
//
//        /**
//         * A graphics processing unit has so-called Video Display Controllers
//         * (or VDC for short) that are image drivers for data from the GPU.
//         * The VDC takes the data from the GPU and encodes it in an abstract format
//         * that defines position, resolution and refresh rate.
//         *
//         * A GPU can drive only up to n-available VDCs, which means that a GPU
//         * can't output more unique images than there are VDCs.
//         *
//         * However, a VDC can mirror the data to another output.
//         *
//         * For example, the Lenovo ThinkPad X220 has 2 VDC's and 8 outputs.
//         * That means that the X220 can display up to 2 unqiue images that define
//         * resolution, position and refresh rate, but those 2 images can be mirrored
//         * and displayed on up-to 8 outputs.
//         *
//         * If you connect a projector to the VGA port and mirror the ThinkPad
//         * screen to that, we are only using a single VDC to drive two outputs.
//         * It is favorable for the outputs to share a common output mode that
//         * both displays support, for ex. 800x600.
//         *
//         * The second VDC is not used and is disabled.
//         *
//         *           +-----------+     +-----------+
//         *           |    VDC    |     |    VDC    |
//         *           |  800x600  |     |    off    |
//         *           |   59 Hz   |     |           |
//         *           +-----+-----+     +-----------+
//         *                 |
//         *         +-------+-------+
//         *         |               |
//         *   +-----+-----+   +-----+-----+
//         *   | ThinkPad  |   | Projector |
//         *   |  Screen   |   |           |
//         *   +-----------+   +-----------+
//         *
//         * However, if you connect a screen to the and "extend" the display area,
//         * you are now using 2 VDC's and each VFC has its parameters defined, such
//         * as resolution, position and refresh rate. The output modes can, but are
//         * usually not common.
//         *
//         *          +-----------+     +-----------+
//         *          |    VDC    |     |    VDC    |
//         *          | 1366x768  |     | 1440x900  |
//         *          |   59 Hz   |     |   75 Hz   |
//         *          +-----+-----+     +-----+-----+
//         *                |                 |
//         *                |                 |
//         *          +-----+-----+     +-----+-----+
//         *          | ThinkPad  |     | External  |
//         *          |  Screen   |     | Monitor   |
//         *          +-----------+     +-----------+
//         *
//         * If running in "mirror" mode, each VDC has its position set to 0,0.
//         * If running in "extend" mode, each VDC has its position set relative to the
//         * primary monitor, which is considered the monitor positioned to 0,0.
//         *
//         */
//        class VideoController {
//        private:
//
//            VideoControllerType *id;
//            VideoControllerInfo *info;
//            ScreenResources *parent;
//            vector<VideoOutput*> *activeOutputs;
//            vector<VideoOutput*> *supportedOutputs;
//
//        public:
//
//            ~VideoController();
//            VideoController(VideoControllerType *id, ScreenResources *resources);
//
//            /**
//             * Get the currently active outputs on the video controller.
//             * @return a list of active video outputs
//             */
//            vector<VideoOutput*> *getActiveOutputs();
//
//            /**
//             * Gets the internal ID of the controller (RRCrtc) used
//             * by the X11 RandR extensions
//             * @return the X11 ID of the output (RRCrtc)
//             */
//            VideoControllerType* getControllerId() const;
//
//            /**
//             * Gets the virtual X position on the display.
//             * @return the virtual X position
//             */
//            [[deprecated("Use getPosition instead of the raw coordinate")]]
//            int getXPosition() const;
//
//            /**
//             * Gets the virtual Y position on the screen.
//             * @return the virtual Y position
//             */
//            [[deprecated("Use getPosition instead of the raw coordinate")]]
//            int getYPosition() const;
//
//            /**
//             * Sets the position of the video output on the screen
//             * @param position the position to set
//             */
//            void setPosition(point position);
//
//            /**
//             * Returns the position of the video output on the screen
//             * @return the position on the screen
//             */
//            point getPosition() const;
//
//            /**
//             * Sets the width of the controller in pixels.
//             * @param param the width to set
//             */
//            void setWidthPixels(unsigned int param);
//
//            /**
//             * Sets the height of the controller in pixels.
//             * @param param the height to set
//             */
//            void seHeightPixels(unsigned int param);
//
//            /**
//             * Sets the output mode of the controller (width, height)
//             * @param pMode sets the output mode
//             */
//            void setOutputMode(VideoOutputMode *pMode);
//
//            /**
//             * Adds an output to be used on the controller
//             *
//             * TODO: implement mirroring (multiple outputs)
//             *
//             * @param output the output to add
//             */
//            void setOutput(VideoOutput *output);
//
//            /**
//             * Resets all the video controllers settings to their default values
//             */
//            void resetConfiguration();
//
//            /**
//             * Check if the output on this controller is active
//             * @return true if the output is active
//             */
//            bool isEnabled() const;
//
//            /**
//             * This returns a list of outputs that are supported by this controller
//             * @return a list of hardware-supported outputs
//             */
//            vector<VideoOutput*> *getSupportedOutputs();
//
//            void setRotation(Rotation i);
//
//            RROutput *getRROutputPointer();
//
//            int getRROutputCount();
//        };
//
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
            vector<VideoController> *controllers;
            vector<VideoOutput*> *videoOutputs;
            vector<VideoOutputModeInfo*> *videoOutputModes;

            vector<VideoController> *availableControllers;

            vector<Monitor*> *monitors;

        public:

            ScreenResources(XServer *server);
            ~ScreenResources();

            /**
             * Get the list of all available controllers
             * @return list of all available controllers
             */
            vector<VideoController> *getControllers() const;

            /**
             * Get the list of all available video outputs
             * @return list of all video outputs
             */
            vector<VideoOutput*> *getVideoOutputs() const;

            /**
             * Get the list of all available video output modes
             * @return list of all output modes
             */
            vector<VideoOutputModeInfo*> *getVideoOutputModes() const;

            /**
             * Get the raw X11 RandR resources, to be used in native X11 RandR
             * API calls
             * @return the raw X11 RandR resources
             */
            XRRScreenResources *getRawResources();

            /**
             * Gets the parent X11 Server of the resources
             * @return the parent X11 server
             */
            XServer *getParentServer() const;

            void markControllerAsBusy(VideoController videoController);

            void releaseController(VideoController i);

            VideoController requestController();
            vector<Monitor*> *getMonitors();

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

            /**
             * Connect to the default X11 server on localhost
             * @return true if the connection succeeded
             */
            bool connect();

            /**
             * The connection to the X server is a singleton
             * @return the singleton XServer instance
             */
            static XServer* getDefaultXServer();

            /**
             * Gets the display of the X11 server
             * @return the X11 display/server
             */
            Display *getDisplay();

            /**
             * Gets the default, root window of X11
             * @return the default, root window
             */
            Window getWindow();
        };
//
//
//        /**
//         * This class is the main interface to the libthinkdock
//         * Display Management library. All manipulations to monitors
//         * are supposed to be done via this class.
//         */
//        class Monitor {
//
//        private:
//
//            VideoOutputMode *videoMode = nullptr;
//            VideoController *videoController = nullptr;
//            VideoOutput *videoOutput = nullptr;
//
//            Monitor *topWing;
//            Monitor *leftWing;
//            Monitor *rightWing;
//            Monitor *bottomWing;
//
//            unsigned int xAxisMaxHeight = 0;
//            unsigned int yAxisMaxWidth = 0;
//
//            unsigned long xAxisMaxHeightmm = 0;
//            unsigned long yAxisMaxWidthmm = 0;
//
//            bool limitsCalculated = false;
//
//            Rotation rotation = RR_Rotate_0;
//
//            vector<Monitor*> *mirrored = new vector<Monitor*>;
//
//            void calculateLimits();
//
//        public:
//
//            void setRightWing(Monitor*);
//            void setLeftWing(Monitor*);
//            void setTopWing(Monitor *);
//            void setBottomWing(Monitor *);
//
//            void setOutputMode(VideoOutputMode *mode);
//
//
//            void setOutput(VideoOutput *output);
//            bool setController(VideoController *pController);
//            VideoController getActiveController() const;
//            bool isControllerSupported(VideoController *pController);
//            void disable(ScreenResources *pResources);
//            string* getName();
//            VideoOutputMode *getPreferredOutputMode() const;
//            VideoOutput* getOutput();
//
//
//            [[deprecated]]
//            unsigned int getTotalWidth();
//            [[deprecated]]
//            unsigned int getTotalHeight();
//
//            point getPrimaryPosition();
//            void calculateMonitorPositions();
//            dimensions getScreenDimensionsPixels();
//            dimensions getScreenDimensionsMillimeters();
//            void applyCascadingConfig(ScreenResources *pResources);
//            void setConfig(Monitor *pMonitor, ScreenResources *pResources);
//            bool isOutputModeSupported(VideoOutputMode *pMode);
//            void setRotation(Rotation rotation);
//            Rotation getRotation();
//            void addMirror(Monitor *pMonitor);
//        };
//
//        class ConfigurationManager {
//        private:
//            vector<Monitor*>* allMonitors;
//            Monitor *primaryMonitor;
//            ScreenResources *resources;
//        public:
//
//            ConfigurationManager(ScreenResources *resources);
//            ~ConfigurationManager();
//
//            void setMonitorPrimary(Monitor *monitor);
//            vector<DisplayManager::Monitor *> *getAllMonitors();
//
//            void commit();
//
//            VideoOutputMode *getCommonOutputMode(Monitor *pMonitor, Monitor *pMonitor1);
//        };

        typedef struct _point {
            int x;
            int y;
        } point;

        typedef struct _dimensions {
            unsigned long width;
            unsigned long height;
        } dimensions;

        class Monitor {

        private:

            ScreenResources *screenResources;

            VideoOutputModeInfo *videoModeInfo = None;

            VideoController videoController = None;
            VideoControllerInfo *videoControllerInfo = None;

            VideoOutput *videoOutput = None;
            VideoOutputInfo *videoOutputInfo = None;

            Monitor *bottomMonitor;
            Monitor *topMonitor;
            Monitor *rightMonitor;
            Monitor *leftMonitor;

        public:

            Monitor(VideoOutput *, ScreenResources *pResources);

            /* State methods */

            void turnOff();
            bool isOff() const;

            /* accessor methods */

            string getInterfaceName() const;
            point getPosition() const;
            VideoOutputMode getPreferredOutputMode() const;

            /* config handlers */

            bool applyConfiguration() const;
            void release();
            bool reconfigure();

            /* config methods */

            void setPosition(point position);

            void setLeftMonitor(Monitor*);
            void setRightMonitor(Monitor*);
            void setTopMonitor(Monitor*);
            void setBottomMonitor(Monitor*);

            void setController(VideoController controller);
            void setOutputMode(VideoOutputMode mode);

            /* info methods */

            bool isOutputModeSupported(VideoOutputMode mode);

            bool isConnected();
        };

    };

};


#endif