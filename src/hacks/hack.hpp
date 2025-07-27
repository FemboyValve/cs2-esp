#pragma once


// Purpose: This is where we render stuff to the screen...
// reason being anywhere else and you will crash the render thread.
namespace hack {
	extern std::vector<std::pair<std::string, std::string>> boneConnections;

	void loop();
}
