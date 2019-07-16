
#include <ESPAsyncWebServer.h>

namespace Blinken {
namespace Networking {
void setup();
extern AsyncWebServer server;
extern AsyncEventSource events;
extern AsyncWebSocket ws;
} // namespace Networking
} // namespace Blinken