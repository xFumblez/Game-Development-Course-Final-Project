# to compile your project, type make and press enter

all: project

project: project.cpp
	g++ project.cpp timers.cpp libggfonts.a -Wall -lX11 -lGL -lGLU -lm -o project -lXext -lpthread

clean:
	rm -f project

