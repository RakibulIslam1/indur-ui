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
#elif INDURUI_EXAMPLE == 7
#include "../../examples/SH1106Display/SH1106Display.ino"
#else
#error Select an INDURUI_EXAMPLE from 1 to 7
#endif
