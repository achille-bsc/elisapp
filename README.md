# Elisapp | Martian navigation application

Elisapp is our new Martian navigation application, designed for future explorers of the Red Planet and adaptable for use by rovers. It operates locally, relying exclusively on a satellite-based positioning system.
The user is able to plan their journey by taking into account all meteorological and topographical data, missions and point of interest along their route, and their resource constraints.
As safety is our top priority, we have designed a head-up display (HUD), integrated directly into the astronaut's visor. This allows them to quickly identify all essential information and react accordingly.

# What ELISAPP does and how it work
Elisapp is installed directly into the helmet and runs on Linux on a Snapdragon chip. 
1. As the astronaut prepare the mission from the Martian base by entering the coordinates of the destination, Elisapp downloads the latest meteorological and topographical data for the route from NASA's database.
2. Elisapp then calculates the most efficient route in 200 to 400 milliseconds for a 35km journey, taking into account the starting and ending points, terrain difficulty and elevation, hazardous areas to avoid, temperature and radiation levels, available oxygen supply, a time limit for the trip and specific missions based on the points of interest they will encounter. It will maintains a minimum safety distance of 50m from the nearest hazardous area and can calculate a route of up to 500 km in less than a second.
3. The astronaut can view the map with the planned route and different filters depending on the information he want to check. When he is ready, he confirms the mission before departure.
4. Three satellites in areostationary orbits triangulate the astronaut's position. Meanwhile, their orientation is determined using a system consisting of two antennas mounted on the astronaut's helmet and two phase-shifted signals. With this, the Elisapp user can navigate safely on Mars and access all their information via a heads-up display.
<br>
In the future, Elisapp could be installed in a rover to enable it to navigate autonomously over very long distances and to modify its own route based on new meteorological data.

## Tools, coding languages, hardware and software
 - C++ 20<br>
 - LVGL <br>
