#include <libthinkpad.h>
#include <iostream>

using ThinkPad::PowerManagement::ACPI;
using ThinkPad::PowerManagement::ACPIEvent;
using ThinkPad::PowerManagement::ACPIEventHandler;

class ACPIHandler : public ACPIEventHandler {

public:

    void handleEvent(ACPIEvent event) {

        switch (event) {

            case ACPIEvent::DOCKED:
                std::cout << "ThinkPad was docked" << std::endl;
                break;
            case ACPIEvent::UNDOCKED:
                std::cout << "ThinkPad was undocked" << std::endl;
                break;
            case ACPIEvent::LID_CLOSED:
                std::cout << "ThinkPad lid was closed" << std::endl;
                break;
            case ACPIEvent::LID_OPENED:
                std::cout << "ThinkPad lid was opened" << std::endl;
                break;

        }

    }

};

int main(void) {


    ACPI *acpi = new ACPI();
    ACPIHandler *handler = new ACPIHandler();

    acpi->addEventHandler(handler);
    acpi->start();
    acpi->wait();

    delete acpi;
    delete handler;

}
