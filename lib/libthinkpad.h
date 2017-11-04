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
#include <cstdio>

#include <X11/extensions/Xrandr.h>

#define IBM_DOCK_DOCKED     "/sys/devices/platform/dock.2/docked"
#define IBM_DOCK_MODALIAS   "/sys/devices/platform/dock.2/modalias"
#define IBM_DOCK_ID         "acpi:IBM0079:PNP0C15:LNXDOCK:\n"
#define ERR_INVALID (-1)

// #define DRYRUN

#define SUSPEND_REASON_LID 0
#define SUSPEND_REASON_BUTTON 1

#define EVENT_UNKNOWN -1
#define EVENT_POWERBUTTON 0
#define EVENT_LID_CLOSE 1
#define EVENT_LID_OPEN 2
#define EVENT_DOCK 3
#define EVENT_UNDOCK 4

#define ACPI_POWERBUTTON "button/power PBTN"
#define ACPI_LID_OPEN "button/lid LID open"
#define ACPI_LID_CLOSE "button/lid LID close"

#define ACPID_SOCK "/var/run/acpid.socket"

#define BUFSIZE 128
#define INBUFSZ 1
#define NAMESZ 128

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
typedef int ACPIEvent;

namespace ThinkPad {

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
    namespace PowerManagement {

        class PowerStateManager;
        class ACPIEventHandler;
        class ACPI;

        struct _ACPIEventMetadata {
            ACPIEvent event;
            ACPIEventHandler *handler;
        };

        typedef struct _ACPIEventMetadata ACPIEventMetadata;

        class PowerStateManager {

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

        /**
         * The ACPI class is used for power event monitoring
         * and reporting. It combines the functionality from
         * udev and acpid into a single API to be used by applications.
         *
         * @brief the ACPI class for power handling
         */
        class ACPI {
        private:

            static void *handle_acpid(void*);
            static void *handle_udev(void*);

            static pthread_t acpid_listener;
            static pthread_t udev_listener;

            ACPIEventHandler *ACPIhandler;

        public:
            ~ACPI();

            /**
             * Set a custom event handler for ACPI events
             *
             * @param handler
             */
            void setEventHandler(ACPIEventHandler *handler);

            /**
             * Block the caller of the method for infinite-loop
             * exit-prevention. Used for testing.
             */
            void wait();
        };

        class ACPIEventHandler {
        public:
            /**
             * PRIVATE METHOD: DO NOT USE
             */
            static void* _handleEvent(void* _this);

            /**
             * This method is called for various ACPI events, such
             * as power button presses, lid events and dock events.
             *
             * @param event the event that occured
             */
            virtual void handleEvent(ACPIEvent event) = 0;
        };

    }

    namespace DisplayManager {

        class XServer;
        class ScreenResources;
        class Monitor;
        typedef struct _point point;

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

        class Monitor {

        private:

            ScreenResources *screenResources;

            VideoOutputModeInfo *videoModeInfo = None;

            VideoController videoController = None;
            VideoControllerInfo *videoControllerInfo = None;

            VideoOutput *videoOutput = None;
            VideoOutputInfo *videoOutputInfo = None;

            Monitor *bottomMonitor = nullptr;
            Monitor *topMonitor = nullptr;
            Monitor *rightMonitor = nullptr;
            Monitor *leftMonitor = nullptr;

            Monitor *mirror = nullptr;

            unsigned int xAxisMaxHeight = 0;
            unsigned int yAxisMaxWidth = 0;

            unsigned long xAxisMaxHeightmm = 0;
            unsigned long yAxisMaxWidthmm = 0;

            unsigned long screenWidth = 0;
            unsigned long screenHeight = 0;

            unsigned long screenWidthMillimeters = 0;
            unsigned long screenHeightMillimeters = 0;

            bool isPrimary = false;

            void calculateLimits();
            void calculateRelativePositions();
            void applyConfiguration(XServer* server, Monitor *monitor);
            void setController(VideoController controller);

            point getPrimaryRelativePosition();

            unsigned int rotateNormalize(unsigned int unknownSize);
            unsigned long rotateNormalizeMillimeters(unsigned long unknownSize);

            VideoOutputMode findCommonOutputMode(Monitor *pMonitor, Monitor *pMonitor1);

        public:

            Monitor(VideoOutput *, ScreenResources *pResources);
            ~Monitor();

            /* State methods */

            void turnOff();
            bool isOff() const;
            bool isConnected();

            /* accessor methods */

            string getInterfaceName() const;
            point getPosition() const;
            VideoOutputMode getPreferredOutputMode() const;

            /* config handlers */

            bool applyConfiguration();
            void release();
            bool reconfigure();

            /* config methods */

            void setPosition(point position);
            void setPrimary(bool i);
            void setRotation(Rotation i);

            void setLeftMonitor(Monitor*);
            void setRightMonitor(Monitor*);
            void setTopMonitor(Monitor*);
            void setBottomMonitor(Monitor*);

            void setMirror(Monitor *pMonitor);

            void setOutputMode(VideoOutputMode mode);

            /* info methods */

            bool isOutputModeSupported(VideoOutputMode mode);

        };

        typedef struct _point {
            int x;
            int y;
        } point;

    }

    struct config_keypair_t {
        char key[128];
        char value[128];
    };

    struct config_section_t {
        char name[128];
        vector<struct config_keypair_t*> *keypairs = nullptr;
    };


    class Configuration {

        vector<struct config_section_t*> *sections = new vector<struct config_section_t*>;

    public:
        ~Configuration();

        vector<struct config_section_t*>* parse(string path);
        void writeConfig(vector<struct config_section_t*> *sections, string path);
    };

}


#endif
