#pragma once
#include "world.h"
#include "vehicle.h"

typedef enum ViewType {Inside, Outside, Global} ViewType;

typedef struct WorldViewer{
  World* world;
  float zoom;
  float camera_z;
  int window_width, window_height;
  Vehicle* self;
  ViewType view_type;
} WorldViewer;

// call this to start the visualization of the stuff.
// This will block the program, and terminate when pressing esc on the viewport
void WorldViewer_runGlobal(World* world,
			   Vehicle* self,
			   int* argc, char** argv);

void WorldViewer_init(WorldViewer* viewer,
		      World* w,
		      Vehicle* self);