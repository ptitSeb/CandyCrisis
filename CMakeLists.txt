cmake_minimum_required(VERSION 2.6)

project(candycrisis)

option(PANDORA "Set to ON if targeting an OpenPandora device" ${PANDORA})
option(USE_FMOD "Set to ON if using FMod instead of SDL_sound" ${USE_FMOD})

if(PANDORA)
	add_definitions(-DPANDORA)
endif()

if(USE_FMOD)
        add_definitions(-DUSE_FMOD)
endif()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-DLINUX=1)
endif()

add_definitions(-std=c++11) 

set(GAME_SOURCE_FILES
	blitter.cpp
	control.cpp
	font.cpp
	gameticks.cpp
	graphics.cpp
	graymonitor.cpp
	grays.cpp
	gworld.cpp
	hiscore.cpp
	keyselect.cpp
	level.cpp
	main.cpp
	moving.cpp
	MTypes.cpp
	music.cpp
	next.cpp
	opponent.cpp
	pause.cpp
	players.cpp
	prefs.cpp
	random.cpp
	score.cpp
	SDLU.cpp
	soundfx.cpp
	stdafx.cpp
	tutorial.cpp
	tweak.cpp
	victory.cpp
	zap.cpp
)


set(THE_LIBS SDL2 SDL2_image)

if(USE_FMOD)
 set(THE_LIBS ${THE_LIBS} fmod)
else()
 set(THE_LIBS ${THE_LIBS} SDL2_sound)
endif()

add_executable(candycrisis ${GAME_SOURCE_FILES})
target_link_libraries(candycrisis ${THE_LIBS})
