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
using std::ostringstream;

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

                if (strstr(buf, ACPI_BUTTON_VOLUME_UP) != NULL) {
                    event = ACPIEvent::BUTTON_VOLUME_UP;
                }

                if (strstr(buf, ACPI_BUTTON_VOLUME_DOWN) != NULL) {
                    event = ACPIEvent::BUTTON_VOLUME_DOWN;
                }

                if (strstr(buf, ACPI_BUTTON_BRIGHTNESS_DOWN) != NULL) {
                    event = ACPIEvent::BUTTON_BRIGHTNESS_DOWN;
                }

                if (strstr(buf, ACPI_BUTTON_BRIGHTNESS_UP) != NULL) {
                    event = ACPIEvent::BUTTON_BRIGHTNESS_UP;
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

                if (strstr(buf, ACPI_DOCK_EVENT) != NULL ||
				strstr(buf, ACPI_DOCK_EVENT2) != NULL) {
                    event = ACPIEvent::DOCKED;
                }

                if (strstr(buf, ACPI_UNDOCK_EVENT) != NULL ||
				strstr(buf, ACPI_UNDOCK_EVENT2) != NULL) {
                    event = ACPIEvent::UNDOCKED;
                }

                if (strstr(buf, ACPI_THERMAL) != NULL) {
                    event = ACPIEvent::THERMAL_ZONE;
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
        return nullptr;
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

                    /*
                     * One could argue that I can use this instead of reading the
                     * file manually but this just plainly does not work, it returns
                     * what it feels like of returning
                     */
                    // const char *docked = udev_device_get_sysattr_value(device, "docked");

                    Hardware::Dock dock;

                    if (!dock.probe()) {
                        fprintf(stderr, "fixme: udev event fired on non-sane dock\n");
                        udev_device_unref(device);
                        continue;
                    }

                    /* Wait for the dock to appear */
                    sleep(1);

                    event = dock.isDocked() ? ACPIEvent::DOCKED : ACPIEvent::UNDOCKED;

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
                udev_device_unref(device);
            }

        }

        return nullptr;

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
        return nullptr;
    }


    /********************** Utilities::Ini *******************/

    Utilities::Ini::Ini::~Ini()
    {
        if (sections == nullptr) return;

        for (IniSection* section : *sections) {
            delete section;
        }

        delete sections;
    }

    vector<Utilities::Ini::IniSection*>* Utilities::Ini::Ini::readIni(std::string path)
    {
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

        if (read(fd, file,  buf.st_size) != buf.st_size) {
            fprintf(stderr, "config: read failed: %s\n", strerror(errno));
            close(fd);
            return nullptr;
        }

        close(fd);

        for(unsigned int i = 0; i < sizeof(file); i++) {

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

            IniSection *section = new IniSection;
            section->keypairs = new vector<IniKeypair*>;

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

                IniKeypair* keypair = new IniKeypair;
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

#define ASSERT_WRITE(write) if (write < 0) { printf("write failed: %s\n", strerror(errno)); return false; }

    bool Utilities::Ini::Ini::writeIni(std::string path)
    {

        int fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);

        if (fd < 0) {
            printf("config: error writing config file: %s\n", strerror(errno));
            return false;
        }

        if (truncate(path.c_str(), 0) < 0) {
            printf("config: truncate failed of old file: %s\n",strerror(errno));
            return false;
        }

        for (IniSection *section : *sections) {

            /* write the section name */
            ASSERT_WRITE(write(fd, "[", 1));
            ASSERT_WRITE(write(fd, section->name, strlen(section->name)));
            ASSERT_WRITE(write(fd, "]\n", 2));

            for (IniKeypair* keypair : *section->keypairs) {

                ASSERT_WRITE(write(fd, keypair->key, strlen(keypair->key)));
                ASSERT_WRITE(write(fd, "=", 1));
                ASSERT_WRITE(write(fd, keypair->value, strlen(keypair->value)));
                ASSERT_WRITE(write(fd, "\n", 1));

            }

            ASSERT_WRITE(write(fd, "\n", 1));

        }

        close(fd);

        return true;

    }

    vector<Utilities::Ini::IniSection *> Utilities::Ini::Ini::getSections(const char *sectionName)
    {

        vector<IniSection*> ret;

        if (sections == nullptr)
            return ret;


        for (IniSection *section : *this->sections) {
            if (strcmp(section->name, sectionName) == 0) {
                ret.push_back(section);
            }
        }

        return ret;

    }

    Utilities::Ini::IniSection *Utilities::Ini::Ini::getSection(const char *section)
    {
        for (IniSection* local : *sections) {
            if (strcmp(local->name, section) == 0) {
                return local;
            }
        }

        return nullptr;
    }

    void Utilities::Ini::Ini::addSection(IniSection *section)
    {
        this->sections->push_back(section);
    }

    Utilities::Ini::IniKeypair::IniKeypair(const char *key, const char *value)
    {
        memset(this->key, 0, sizeof(this->key));
        memset(this->value, 0, sizeof(this->value));

        strcpy(this->key, key);
        strcpy(this->value, value);
    }

    Utilities::Ini::IniKeypair::IniKeypair()
    {
        memset(this->key, 0, sizeof(this->key));
        memset(this->value, 0, sizeof(this->value));
    }

    Utilities::Ini::IniSection::~IniSection()
    {
        for (IniKeypair *keypair : *keypairs) {
            delete keypair;
        }

        delete keypairs;
    }

    Utilities::Ini::IniSection::IniSection()
    {
        memset(this->name, 0, sizeof(this->name));
    }

    Utilities::Ini::IniSection::IniSection(const char *name)
    {
        memset(this->name, 0, sizeof(this->name));
        strcpy(this->name, name);
        this->keypairs = new vector<IniKeypair*>;
    }

    const char *Utilities::Ini::IniSection::getString(const char *key) const
    {
        for (IniKeypair* keypair : *keypairs) {
            if (strcmp(keypair->key, key) == 0) {
                return keypair->value;
            }
        }

        return nullptr;
    }

    const void Utilities::Ini::IniSection::setString(const char *key, const char *value)
    {
        IniKeypair *keypair = new IniKeypair(key, value);
        this->keypairs->push_back(keypair);
    }

    const int Utilities::Ini::IniSection::getInt(const char *key) const
    {
        const char *string = getString(key);

        if (string == nullptr) {
            return INT32_MIN;
        }

        return atoi(string);
    }

    const void Utilities::Ini::IniSection::setInt(const char *key, const int value)
    {
        ostringstream stream;
        stream << value;
        setString(key, stream.str().c_str());
    }

    const vector<int> Utilities::Ini::IniSection::getIntArray(const char *key) const
    {
        string lenStr = string(key);
        lenStr.append("_len");

        const int len = getInt(lenStr.c_str());

        vector<int> ints;

        for (int i = 0; i < len; i++) {

            ostringstream stream;

            stream << key;
            stream << "_";
            stream << i;

            string test = stream.str();

            const int num = getInt(test.c_str());

            ints.push_back(num);

        }

        return ints;

    }

    const void Utilities::Ini::IniSection::setIntArray(const char *key, vector<int> *values)
    {

        ostringstream stream;
        stream << key;
        stream << "_len";

        setInt(stream.str().c_str(), (int) values->size());

        for (unsigned int i = 0; i < values->size(); i++) {
            stream = ostringstream();
            stream << key;
            stream << "_";
            stream << i;
            setInt(stream.str().c_str(), values->at(i));
        }

    }

    const void Utilities::Ini::IniSection::setStringArray(const char *key, const vector<const char *> *strings)
    {

        ostringstream stream;
        stream << key;
        stream << "_len";

        setInt(stream.str().c_str(), (int) strings->size());

        for (unsigned int i = 0; i < strings->size(); i++) {
            stream = ostringstream();
            stream << key;
            stream << "_";
            stream << i;
            setString(stream.str().c_str(), strings->at(i));
        }

    }

    const vector<const char *> Utilities::Ini::IniSection::getStringArray(const char *key)
    {

        vector<const char*> strings;

        ostringstream stream;
        stream << key;
        stream << "_len";

        const int len = getInt(stream.str().c_str());

        for (int i = 0; i < len; i++) {

            stream = ostringstream();
            stream << key;
            stream << "_";
            stream << i;

            const char *string = getString(stream.str().c_str());
            strings.push_back(string);

        }

        return strings;

    }


    /******************** ThinkLight **********************/

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

        return -1;
    }

    void Hardware::Backlight::setBrightness(System system, int value)
    {
        switch (system) {
            case Backlight::System ::NVIDIA:
                Utilities::CommonUtils::intWrite(SYSFS_BACKLIGHT_NVIDIA"/brightness", value);
                break;
            case Backlight::System ::INTEL:
                Utilities::CommonUtils::intWrite(SYSFS_BACKLIGHT_INTEL"/brightness", value);
                break;
        }
    }

    int Hardware::Backlight::getMaxBrightness(Hardware::Backlight::System system) {

        int maxBrightness = -1;

        switch (system) {
            case Backlight::System::INTEL: {
                maxBrightness = Utilities::CommonUtils::intRead(SYSFS_BACKLIGHT_INTEL"/max_brightness");
                break;
            }
            case Backlight::System::NVIDIA: {
                maxBrightness = Utilities::CommonUtils::intRead(SYSFS_BACKLIGHT_NVIDIA"/max_brightness");
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
                return Utilities::CommonUtils::intRead(SYSFS_BACKLIGHT_INTEL"/brightness");
            }
            case Backlight::System::NVIDIA: {
                return Utilities::CommonUtils::intRead(SYSFS_BACKLIGHT_NVIDIA"/brightness");
            }
        }

        if (brightness < 0) {
            fprintf(stderr, "backlight: error reading backlight\n");
        }

        return brightness;

    }


    /************************* BatteryManager **************************/
#if 0

    int Hardware::BatteryManager::getChargeStartThreshold(BatteryID battery)
    {
        switch (battery) {
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::intRead(SYSFS_BATTERY_PRIMARY"/charge_start_threshold");
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::intRead(SYSFS_BATTERY_SECONDARY"/charge_start_threshold");
        }

        return -EINVAL;
    }

    int Hardware::BatteryManager::getChargeStopThreshold(BatteryID battery)
    {
        switch (battery) {
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::intRead(SYSFS_BATTERY_PRIMARY"/charge_stop_threshold");
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::intRead(SYSFS_BATTERY_SECONDARY"/charge_stop_threshold");
        }

        return -EINVAL;
    }

    int Hardware::BatteryManager::setChargeStopThreshold(BatteryID battery, int value)
    {
        switch(battery) {
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::intWrite(SYSFS_BATTERY_PRIMARY"/charge_stop_threshold", value);
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::intWrite(SYSFS_BATTERY_SECONDARY"/charge_stop_threshold", value);
        }

        return -EINVAL;
    }

    int Hardware::BatteryManager::setChargeStartThreshold(BatteryID battery,  int value)
    {
        switch(battery){
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::intWrite(SYSFS_BATTERY_PRIMARY"/charge_start_threshold", value);
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::intWrite(SYSFS_BATTERY_SECONDARY"/charge_stop_threshold", value);
        }

        return -EINVAL;
    }

    bool Hardware::BatteryManager::probe(BatteryID battery)
    {
        switch (battery) {
        case BatteryID::BATTERY_PRIMARY:
            return !access(SYSFS_BATTERY_PRIMARY"/present", R_OK);
        case BatteryID::BATTERY_SECONDARY:
            return !access(SYSFS_BATTERY_SECONDARY"/present", R_OK);
        }

        return false;
    }

    const char *Hardware::BatteryManager::getFRU(BatteryID battery)
    {
        switch (battery) {
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::fileRead(SYSFS_BATTERY_PRIMARY"/model_name");
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::fileRead(SYSFS_BATTERY_SECONDARY"/model_name");
        }

        return nullptr;
    }

    const char *Hardware::BatteryManager::getOEM(BatteryID battery)
    {
        switch (battery) {
        case BatteryID::BATTERY_PRIMARY:
            return Utilities::CommonUtils::fileRead(SYSFS_BATTERY_PRIMARY"/manufacturer");
        case BatteryID::BATTERY_SECONDARY:
            return Utilities::CommonUtils::fileRead(SYSFS_BATTERY_SECONDARY"/manufacturer");
        }

        return nullptr;
    }
#endif

    /********************** CommonUtils **********************/

    const char *Utilities::CommonUtils::fileRead(const char *path) {

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

    const int Utilities::CommonUtils::intRead(const char *path)
    {
        const char *data = fileRead(path);

        if (data == NULL)
            return -EIO;

        int value = atoi(data);
        free((void*)data);
        return value;
    }

    int Utilities::CommonUtils::intWrite(const char *path, int value)
    {

        int fd = open(path, O_WRONLY);

        if (fd < 0) {
            fprintf(stderr, "brightnes: error opening file for writing: %s\n", strerror(errno));
            return 1;
        }

        /**
         * The logarithm of a number with base 10 returns the
         * rough number of digits, we add +1 for the number of
         * digits, +1 for the null terminator safety
         *
         * we also need to check if the value is 0; log(0) is not defined
         */
        size_t strlen = value == 0 ? 2 : (size_t) log10f(value) + 2;

        char buf[strlen];
        memset(buf, 0, strlen);
        snprintf(buf, strlen, "%d", value);

        if (write(fd, buf, strlen) < 0) {
            fprintf(stderr, "thinkpad: error writing to file: %s\n", strerror(errno));
            return 1;
        }

        close(fd);

        return 0;

    }

}


