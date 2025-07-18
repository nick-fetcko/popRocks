/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
  Vamp feature extraction plugin for the BeatRoot beat tracker.

  Centre for Digital Music, Queen Mary, University of London.
  This file copyright 2011 Simon Dixon, Chris Cannam and QMUL.
    
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#ifndef _EVENT_H_
#define _EVENT_H_

#include <list>

struct Event {
    double time;
    double beat;
    double salience;

    Event() : time(0), beat(0), salience(0) { }
    Event(double t, double b, double s) : time(t), beat(b), salience(s) { }

    bool operator==(const Event &e) {
	return (time == e.time && beat == e.beat && salience == e.salience);
    }
    bool operator!=(const Event &e) {
	return !operator==(e);
    }
};

typedef std::list<Event> EventList;

#endif

