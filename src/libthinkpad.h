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

#define LIBTHINKPAD_MAJOR 2
#define LIBTHINKPAD_MINOR 3

#include <string>
#include <vector>
#include <cstdio>

#define IBM_DOCK "/sys/devices/platform/dock.2"
#define IBM_DOCK_DOCKED     "/sys/devices/platform/dock.2/docked"
#define IBM_DOCK_MODALIAS   "/sys/devices/platform/dock.2/modalias"
#define IBM_DOCK_ID         "acpi:IBM0079:PNP0C15:LNXDOCK:\n"
#define ERR_INVALID (-1)

// #define DRYRUN

#define ACPI_POWERBUTTON "button/power PBTN"
#define ACPI_LID_OPEN "button/lid LID open"
#define ACPI_LID_CLOSE "button/lid LID close"

#define ACPI_BUTTON_BRIGHTNESS_UP "video/brightnessup BRTUP"
#define ACPI_BUTTON_BRIGHTNESS_DOWN "video/brightnessdown BRTDN"
#define ACPI_BUTTON_VOLUME_UP "button/volumeup VOLUP"
#define ACPI_BUTTON_VOLUME_DOWN "button/volumedown VOLDN"
#define ACPI_BUTTON_MICMUTE "button/f20 F20"
#define ACPI_BUTTON_MUTE "button/mute MUTE"
#define ACPI_BUTTON_THINKVANTAGE "button/prog1 PROG1"
#define ACPI_BUTTON_FNF2_LOCK "button/screenlock SCRNLCK"
#define ACPI_BUTTON_FNF3_BATTERY "button/battery BAT"
#define ACPI_BUTTON_FNF4_SLEEP "button/sleep SBTN"
#define ACPI_BUTTON_FNF5_WLAN "button/wlan WLAN"
#define ACPI_BUTTON_FNF7_PROJECTOR "video/switchmode VMOD"
#define ACPI_BUTTON_FNF12_HIBERNATE "button/suspend SUSP"

#define ACPID_SOCK "/var/run/acpid.socket"

#define SYSFS_THINKLIGHT "/sys/class/leds/tpacpi::thinklight/brightness"
#define SYSFS_MACHINECHECK "/sys/devices/system/machinecheck/machinecheck"

#define SYSFS_BACKLIGHT_NVIDIA "/sys/class/backlight/nv_backlight"
#define SYSFS_BACKLIGHT_INTEL "/sys/class/backlight/intel_backlight"

#define BUFSIZE 128
#define INBUFSZ 1

using std::string;
using std::vector;

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
             * @brief Check if the ThinkPad is physically docked
             * into the UltraDock or the UltraBase
             * @return true if the ThinkPad is inside the dock
             */
            bool isDocked();

            /**
             * @brief Probes the dock if it is an IBM dock and if the
             * dock is sane and ready for detection/state changes
             * @return true if the dock is sane and valid
             */
            bool probe();

        };


        /**
         * @brief The ThinkLight class is ued to probe for the ThinkLight state
         * and validity
         */
        class ThinkLight {
        public:
            /**
             * @brief check if the ThinkLight is currently on
             * @return true if the ThinkLight is on
             */
            bool isOn();

            /**
             * @brief probe the ThinkLight driver for validity
             * @return true if the thinklight driver is sane and ready
             */
            bool probe();

        };
        
        /**
         * @brief The backlight class is used to control the backlight
         * level on the integrated laptop screen
         */
        class Backlight {
        private:

            enum System {
                NVIDIA, INTEL
            };

            int getMaxBrightness(System system);
            int getCurrentBrightness(System system);
            void setBrightness(System system, int value);
            const char *fileRead(const char *path);

        public:

            /**
             * @brief Set the backlight to the specified factor of illumination
             * @param factor the factor to set (0.0 - 1.0)
             */
            void setBacklightLevel(float factor);

            /**
             * @brief Get the current value of the illumination factor
             * @return the current brightness factor
             */
            float getBacklightLevel();
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
             * The ThinkPad is entering the ACPI S3/S4 state
             */
            POWER_S3S4_ENTER,

            /**
             * The ThinkPad is exiting the ACPI S3/S4 state
             */
            POWER_S3S4_EXIT,

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
            BUTTON_POWER,

            /**
             * The volume up button on the ThinkPad or a external device has been pressed
             */
            BUTTON_VOLUME_UP,

            /**
             * The volume down button on the ThinkPad or external device has been pressed
             */
            BUTTON_VOLUME_DOWN,

            /**
             * The Mic mute button on the ThinkPad has been pressed
             */
            BUTTON_MICMUTE,

            /**
             * The mute button on the ThinkPad has been pressed
             */
            BUTTON_MUTE,

            /**
             * The ThinkVantage on the ThinkPad has been pressed
             */
            BUTTON_THINKVANTAGE,

            /**
             * The Fn+F2 Key combination on the ThinkPad has been pressed (lock)
             */
            BUTTON_FNF2_LOCK,

            /**
             * The Fn+F3 Key combination on the ThinkPad has been pressed (battery)
             */
            BUTTON_FNF3_BATTERY,

            /**
             * The Fn+F4 Key combination on the ThinkPad has been pressed (sleep)
             */
            BUTTON_FNF4_SLEEP,

            /**
             * The Fn+F5 Key combination on the ThinkPad has been pressed (WLAN)
             */
            BUTTON_FNF5_WLAN,

            /**
             * The Fn+F7 Key combination on the ThinkPad has been pressed (projector)
             */
            BUTTON_FNF7_PROJECTOR,

            /**
             * The Fn+F12 Key combination on the ThinkPad has been pressed (suspend)
             */
            BUTTON_FNF12_SUSPEND,

            /**
             * An unknown ACPI event has occured
             */
            UNKNOWN,

            /*
             * The brightness decrease button on the ThinkPad has been pressed
             */
            BUTTON_BRIGHTNESS_DOWN,

            /*
             * The brightness increase button on the ThinkPad has been pressed
             */
            BUTTON_BRIGHTNESS_UP
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

            vector<ACPIEventHandler*> *ACPIhandlers;

            bool udev_running = true;

        public:

            ACPI();
            ~ACPI();

            /**
             * @brief Set a custom event handler for ACPI events
             *
             * @param handler
             */
            void addEventHandler(ACPIEventHandler *handler);

            /**
             * @brief Block the caller of the method for infinite-loop
             * exit-prevention. Used for testing.
             */
            void wait();

            /**
             * @brief starts the listening on ACPI events
             */
            void start();
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
     * @brief This part of the library contains various help classes
     * such as ini readers and writers, parsers and converters
     */
    namespace Utilities {

        /**
         * @brief This namespace is a ini/conf/desktop file reader/writer
         */
        namespace Ini {

            /**
             * @brief Defines a keypair in a .ini file
             */
            class IniKeypair
            {
            public:

                char key[128];
                char value[128];

                /**
                 * @brief construct a new keypair
                 * @param key the key to set
                 * @param value the value to set
                 */
                IniKeypair(const char *key, const char *value);
                IniKeypair();

            };


            class IniSection
            {
            public:

                ~IniSection();

                /**
                 * @brief construct a new ini section
                 */
                IniSection();

                /**
                 * @brief construct a new ini section with the
                 * name pre-defined
                 * @param name
                 */
                IniSection(const char *name);

                char name[128];
                vector<IniKeypair*> *keypairs = nullptr;

                /**
                 * @brief get a string from the section
                 * @param key the key of the string
                 * @return the string or nullptr
                 */
                const char *getString(const char *key) const;

                /**
                 * @brief set a string in the section with the key
                 * @param key the key to set
                 * @param value the value to set
                 */
                const void setString(const char *key, const char *value);

                /**
                 * @brief get an int from the section
                 *
                 * WARNING: this uses atoi() so be careful
                 *
                 * @param key the key of the int
                 * @return the int itself
                 */
                const int getInt(const char *key) const;

                /**
                 * @brief set a int in the section with the key
                 * @param key the key to set
                 * @param value the value to set
                 */
                const void setInt(const char *key, const int value);

                /**
                 * @brief Get an array (vector) of ints from the section
                 * @param key the key of the array
                 * @return the vector with the values or an empty vector if no keys are present
                 */
                const vector<int> getIntArray(const char *key) const;

                /**
                 * @brief Set an array (vector) of ints into the section
                 * @param key the key of the array to set
                 * @param values the vector with the values
                 */
                const void setIntArray(const char *key, vector<int> *values);

                /**
                 * @brief Get an array (vector) of strings from the section
                 * @param key the key of the array
                 * @return the vector with the values
                 */
                const void setStringArray(const char *key, const vector<const char*> *strings);

                /**
                 * @brief Get an array (vector) of strings from the section
                 * @param key the key of the array
                 * @return the vector with the values or an empty vector if no keys are present
                 */
                const vector<const char*> getStringArray(const char *key);
            };


            /**
             * @brief This class represents a .ini/.conf/.desktop file parser
             * based on the Windows INI standard.
             *
             * WARNING: COMMENTS ARE NOT SUPPORTED!
             */
            class Ini
            {
                vector<IniSection*> *sections = new vector<IniSection*>;

            public:
                ~Ini();

                /**
                 * @brief parse parse a config file from the disk into the class
                 * @param path the path to the file to parse
                 * @return the point to the section list
                 */
                vector<IniSection*>* readIni(string path);

                /**
                 * @brief writeConfig write a list of sections to the disk
                 * @param sections the list of sections to write
                 * @param path the path to write
                 */
                bool writeIni(string path);


                /**
                 * Get a list of sections from the file with the same name
                 * @param section the name of the sections
                 * @return the section or empry vector
                 */
                vector<IniSection*> getSections(const char *section);

                /**
                 * Get a single section from the file with the name
                 * @param section the section to get
                 * @return the section in the file or null
                 */
                IniSection* getSection(const char *section);

                /**
                 * Add a section to the config file
                 * @param section the section to add
                 */
                void addSection(IniSection* section);
            };

        }

        /**
         * @brief Class used for programs to access the
         * library version
         */
        class Versioning {
        public:

            /**
             * @brief The major version of the library.
             * If this changes, the ABI compat is broken
             * @return the major version
             */
            static int getMajorVersion() {
                return LIBTHINKPAD_MAJOR;
            }

            /**
             * @brief The minor version of the library
             * @return the minor version
             */
            static int getMinorVersion() {
                return LIBTHINKPAD_MINOR;
            }

        };

    }

}


#endif
