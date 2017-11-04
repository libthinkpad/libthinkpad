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

/**
 * @brief The main libthinkpad interface. This contains all the libthinkpad features.
 */
namespace ThinkPad {

    /**
     * @brief This namespace handles ThinkPad hardware, such as docks, lights and batteries.
     */
    namespace Hardware {

        /**
         * @brief The Dock class is used to probe for the dock
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


    }


    /**
     * @brief The power management interfaces. Here you can find ACPI event handlers
     * and power management state configurators and handlers
     */
    namespace PowerManagement {

        class PowerStateManager;
        class ACPIEventHandler;
        class ACPI;

        /**
         * @brief Varoius ACPI events that can occur on the ThinkPad
         */
        enum ACPIEvent {

            /**
             * The lid has been physically closed
             */
            LID_CLOSED,

            /**
             * The lid has been physically opened
             */
            LID_OPENED,

            /**
             * The ThinkPad has been docked into a UltraDock/UltraBase
             */
            DOCKED,

            /**
             * The ThinkPad has been docked into a UltraDock/UltraBase
             */
            UNDOCKED,

            /**
             * The power button on the ThinkPad or the Dock has been pressed
             */
            POWERBUTTON,

            /**
             * An unknown ACPI event has occured
             */
            UNKNOWN

        };

        /**
         * @brief this defines the reason why a system suspend was requested
         */
        enum SuspendReason {
            LID,
            BUTTON
        };

        /**
         * @brief Private internal API metada, do not use
         */
        struct _ACPIEventMetadata {
            ACPIEvent event;
            ACPIEventHandler *handler;
        };

        typedef struct _ACPIEventMetadata ACPIEventMetadata;

        /**
         * The power state manager is used to request power
         * state changes to the system. You can request the system
         * to suspend only currently.
         *
         * @brief Class for hardware power management
         */
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
            static bool requestSuspend(SuspendReason reason);

        };

        /**
         * The ACPI class is used for power event monitoring
         * and reporting. It combines the functionality from
         * udev and acpid into a single API to be used by applications.
         *
         * @brief This handles the system power state and ACPI event dispatches.
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


        /**
         * @brief This is the abstract ACPI event handler class.
         *
         * If you want to use this class, override the handleEvent(ACPIEvent)
         * method and do your thing there. The method is called from another thread
         * so watch out for threading issues that might occur.
         *
         * If you need to
         * use shared resources inside the handler, use the pthread mutex API.
         */
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

    /**
     * @brief The display management and configuration interfaces are found here.
     */
    namespace DisplayManagement {

        class XServer;
        class ScreenResources;
        class Monitor;

        /**
          * @brief This represents a X-Y coordinate point
          */
        typedef struct _point point;

        /**
         * The ScreenResources class is actually the main container
         * of all the objects used by the display management system.
         * The ScreenResources class contains all the output modes, the
         * controllers and the physical outputs.
         *
         * @brief The X11 screen resources unified to one interface
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
             * @brief Get the list of all available controllers
             * @return list of all available controllers
             */
            vector<VideoController> *getControllers() const;

            /**
             * @brief Get the list of all available video outputs
             * @return list of all video outputs
             */
            vector<VideoOutput*> *getVideoOutputs() const;

            /**
             * @brief Get the list of all available video output modes
             * @return list of all output modes
             */
            vector<VideoOutputModeInfo*> *getVideoOutputModes() const;

            /**
             * @brief Get the raw X11 RandR resources, to be used in native X11 RandR
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
         * @brief The XServer class is the main connection and relation
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
             * @brief Connect to the default X11 server on localhost
             * @return true if the connection succeeded
             */
            bool connect();

            /**
             * @brief The connection to the X server is a singleton
             * @return the singleton XServer instance
             */
            static XServer* getDefaultXServer();

            /**
             * @brief Gets the display of the X11 server
             * @return the X11 display/server
             */
            Display *getDisplay();

            /**
             * @brief Gets the default, root window of X11
             * @return the default, root window
             */
            Window getWindow();
        };

        /**
         * This class is used to manage the X11 monitor
         * configuration and position using the RandR extension.
         *
         * @brief The monitor management interface
         */
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

            /**
             * @brief construct a new monitor from a VideoOutput (RROutput)
             * @param pResources the screen resources
             */
            Monitor(VideoOutput *, ScreenResources *pResources);
            ~Monitor();

            /* State methods */

            /**
             * @brief turns off the monitor. This does NOT release the controller.
             */
            void turnOff();

            /**
             * @brief check if the monitor is off
             * @return true if the monitor is off
             */
            bool isOff() const;

            /**
             * @brief check if the monitor is physically connected to the
             * output device
             * @return true if the monitor is connected
             */
            bool isConnected();

            /* accessor methods */

            /**
             * @brief get the physical interface name of the monitor.
             * For example: LVDS-1, DP-1, eDP-1 etc.
             * @return the string representing the interface name
             */
            string getInterfaceName() const;

            /**
             * @brief get the monitor position on the virtual screen
             * @return the position on the screen
             */
            point getPosition() const;

            /**
             * @brief returns the preferred output mode for the monitor. This is usually the
             * native resolution.
             * @return the preferred output mode
             */
            VideoOutputMode getPreferredOutputMode() const;

            /* config handlers */

            /**
             * @brief apply the pending configurations. this method is cascading, which
             * means that you do NOT need to apply config manually to the child screens
             * @return
             */
            bool applyConfiguration();

            /**
             * @brief this releases a controller back and makes the screen invalid, to
             * use the screen again you need to call reconfigure()
             */
            void release();

            /**
             * @brief this requests a controller from the resources and configures the
             * screen to be used again
             *
             * If there is no controller available, the monitor WILL NOT and CAN NOT
             * be used.
             *
             * @return true if there is a controller available, false otherwise
             */
            bool reconfigure();

            /* config methods */

            /**
             * @brief sets the position on the virtual screen
             * @param position the position to set
             */
            void setPosition(point position);

            /**
             * @brief sets this monitor as the primary monitor
             * @param the state to set
             */
            void setPrimary(bool i);

            /**
             * @brief sets the rotation of the monitor
             * @param the rotation to set
             */
            void setRotation(Rotation i);

            /**
             * @brief set the left monitor respectively to the parent
             */
            void setLeftMonitor(Monitor*);

            /**
             * @brief set the right monitor respectively to the parent
             */
            void setRightMonitor(Monitor*);

            /**
             * @brief set the top monitor respectively to the parent
             */
            void setTopMonitor(Monitor*);

            /**
             * @brief set the bottom monitor respectively to the parent
             */
            void setBottomMonitor(Monitor*);

            /**
             * @brief mirror this output to the parameter
             * @param the monitor to mirror to
             */
            void setMirror(Monitor *pMonitor);

            /**
             * @brief sets the output mode for the monitor
             * @param mode the mode to set
             */
            void setOutputMode(VideoOutputMode mode);

            /* info methods */

            bool isOutputModeSupported(VideoOutputMode mode);

        };


        typedef struct _point {
            /**
             * @brief x the X coordinate of the point
             */
            int x;

            /**
             * @brief y tge Y coordinate of the point
             */
            int y;
        } point;

    }

    /**
     * @brief a configuration keypair. Max size is 128 chars
     */
    struct config_keypair_t {
        char key[128];
        char value[128];
    };

    /**
     * This represents a section within a configuration file.
     * Sections are denoted by the square brackets. For example:
     *
     * [Section1]
     *
     * @brief a configuration section. Max size is 128 chars
     */
    struct config_section_t {
        char name[128];
        vector<struct config_keypair_t*> *keypairs = nullptr;
    };


    /**
     * This class is used to manage, read and write config files
     *
     * @brief class used for config file management
     */
    class Configuration {

        vector<struct config_section_t*> *sections = new vector<struct config_section_t*>;

    public:
        ~Configuration();

        /**
         * @brief parse parse a config file from the disk into the class
         * @param path the path to the file to parse
         * @return the point to the section list
         */
        vector<struct config_section_t*>* parse(string path);

        /**
         * @brief writeConfig write a list of sections to the disk
         * @param sections the list of sections to write
         * @param path the path to write
         */
        void writeConfig(vector<struct config_section_t*> *sections, string path);
    };

}


#endif
