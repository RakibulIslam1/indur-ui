#if INDURUI_EXAMPLE == 1
#include "../../examples/QuickStart/QuickStart.ino"
#elif INDURUI_EXAMPLE == 2
#include "../../examples/TwoButtons/TwoButtons.ino"
#elif INDURUI_EXAMPLE == 3
#include "../../examples/PersistentSettings/PersistentSettings.ino"
#elif INDURUI_EXAMPLE == 4
#include "../../examples/RobotDashboard/RobotDashboard.ino"
#elif INDURUI_EXAMPLE == 5
#include "../../examples/CustomIcons/CustomIcons.ino"
#elif INDURUI_EXAMPLE == 6
#include "../../examples/MiniRobotTest/MiniRobotTest.ino"
#else
#error Select an INDURUI_EXAMPLE from 1 to 6
#endif
