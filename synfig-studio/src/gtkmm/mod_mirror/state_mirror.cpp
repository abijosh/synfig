/* === S Y N F I G ========================================================= */
/*!	\file state_mirror.cpp
**	\brief Template File
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>

#include <synfig/valuenode_dynamiclist.h>
#include <synfigapp/action_system.h>

#include "state_mirror.h"
#include "../canvasview.h"
#include "../workarea.h"
#include "../app.h"

#include <synfigapp/action.h>
#include "../event_mouse.h"
#include "../event_layerclick.h"
#include "../toolbox.h"
#include "../dialog_tooloptions.h"
#include <gtkmm/optionmenu.h>
#include "../duck.h"
#include <synfigapp/main.h>

#include "../general.h"

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;
using namespace studio;

/* === M A C R O S ========================================================= */

enum Axis {
	AXIS_X,
	AXIS_Y
} ;

/* === G L O B A L S ======================================================= */

StateMirror studio::state_mirror;

/* === C L A S S E S & S T R U C T S ======================================= */

class DuckDrag_Mirror : public DuckDrag_Base
{
	synfig::Vector center;

	std::vector<synfig::Vector> positions;

public:
	Axis axis;

	DuckDrag_Mirror();
	void begin_duck_drag(Duckmatic* duckmatic, const synfig::Vector& begin);
	bool end_duck_drag(Duckmatic* duckmatic);
	void duck_drag(Duckmatic* duckmatic, const synfig::Vector& vector);
};

class studio::StateMirror_Context : public sigc::trackable
{
	etl::handle<CanvasView> canvas_view_;

	synfigapp::Settings& settings;

	sigc::connection keypress_connect;
	sigc::connection keyrelease_connect;

	etl::handle<DuckDrag_Mirror> duck_dragger_;

	Gtk::Table options_table;

	Gtk::RadioButton::Group radiobutton_group;
	Gtk::RadioButton radiobutton_axis_x;
	Gtk::RadioButton radiobutton_axis_y;

public:

	Axis get_axis()const { return radiobutton_axis_x.get_active()?AXIS_X:AXIS_Y; }
	void set_axis(Axis a)
	{
		if(a==AXIS_X)
			radiobutton_axis_x.set_active(true);
		else
			radiobutton_axis_y.set_active(true);

		duck_dragger_->axis=get_axis();
	}

	void update_axes()
	{
		duck_dragger_->axis=get_axis();
	}

	Smach::event_result event_refresh_tool_options(const Smach::event& x);

	void refresh_tool_options();

	StateMirror_Context(CanvasView* canvas_view);

	~StateMirror_Context();

	const etl::handle<CanvasView>& get_canvas_view()const{return canvas_view_;}
	etl::handle<synfigapp::CanvasInterface> get_canvas_interface()const{return canvas_view_->canvas_interface();}
	synfig::Canvas::Handle get_canvas()const{return canvas_view_->get_canvas();}
	WorkArea * get_work_area()const{return canvas_view_->get_work_area();}

	void load_settings();
	void save_settings();

	bool key_event(GdkEventKey *event);
};	// END of class StateMirror_Context

/* === M E T H O D S ======================================================= */

StateMirror::StateMirror():
	Smach::state<StateMirror_Context>("mirror")
{
	insert(event_def(EVENT_REFRESH_TOOL_OPTIONS,&StateMirror_Context::event_refresh_tool_options));
}

StateMirror::~StateMirror()
{
}

void
StateMirror_Context::load_settings()
{
	String value;

	settings.get_value("mirror.axis",value);
	set_axis((Axis)atoi(value.c_str()));
}

void
StateMirror_Context::save_settings()
{
	settings.set_value("mirror.lock_aspect",strprintf("%d",(int)get_axis()));
}

StateMirror_Context::StateMirror_Context(CanvasView* canvas_view):
	canvas_view_(canvas_view),
	settings(synfigapp::Main::get_selected_input_device()->settings()),
	duck_dragger_(new DuckDrag_Mirror()),
	radiobutton_axis_x(radiobutton_group,_("Horizontal")),
	radiobutton_axis_y(radiobutton_group,_("Vertical"))
{
	// Set up the tool options dialog
	options_table.attach(*manage(new Gtk::Label(_("Mirror Tool"))), 0, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	options_table.attach(radiobutton_axis_x, 0, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	options_table.attach(radiobutton_axis_y, 0, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	options_table.attach(*manage(new Gtk::Label(_("(Shift key toggles axis)"))), 0, 2, 3, 4, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);

	radiobutton_axis_x.signal_toggled().connect(sigc::mem_fun(*this,&StateMirror_Context::update_axes));
	radiobutton_axis_y.signal_toggled().connect(sigc::mem_fun(*this,&StateMirror_Context::update_axes));

	options_table.show_all();
	refresh_tool_options();
	App::dialog_tool_options->present();

	get_work_area()->set_allow_layer_clicks(true);
	get_work_area()->set_duck_dragger(duck_dragger_);

	keypress_connect=get_work_area()->signal_key_press_event().connect(sigc::mem_fun(*this,&StateMirror_Context::key_event),false);
	keyrelease_connect=get_work_area()->signal_key_release_event().connect(sigc::mem_fun(*this,&StateMirror_Context::key_event),false);

//	get_canvas_view()->work_area->set_cursor(Gdk::CROSSHAIR);
	get_canvas_view()->work_area->reset_cursor();

	App::toolbox->refresh();

	set_axis(AXIS_X);
	load_settings();
}

bool
StateMirror_Context::key_event(GdkEventKey *event)
{
	if (event->keyval==GDK_Shift_L || event->keyval==GDK_Shift_R )
		set_axis(get_axis()==AXIS_X ? AXIS_Y:AXIS_X);

	return false; //Pass on the event to other handlers, just in case
}

void
StateMirror_Context::refresh_tool_options()
{
	App::dialog_tool_options->clear();
	App::dialog_tool_options->set_widget(options_table);
	App::dialog_tool_options->set_local_name(_("Mirror Tool"));
	App::dialog_tool_options->set_name("mirror");
}

Smach::event_result
StateMirror_Context::event_refresh_tool_options(const Smach::event& /*x*/)
{
	refresh_tool_options();
	return Smach::RESULT_ACCEPT;
}


StateMirror_Context::~StateMirror_Context()
{
	save_settings();

	get_work_area()->clear_duck_dragger();
	get_canvas_view()->work_area->reset_cursor();

	keypress_connect.disconnect();
	keyrelease_connect.disconnect();

	App::dialog_tool_options->clear();

	App::toolbox->refresh();
}

DuckDrag_Mirror::DuckDrag_Mirror():
	axis(AXIS_X)
{
}

#ifndef EPSILON
#define EPSILON	0.0000001
#endif

void
DuckDrag_Mirror::begin_duck_drag(Duckmatic* duckmatic, const synfig::Vector& /*offset*/)
{
	const DuckList selected_ducks(duckmatic->get_selected_ducks());
	DuckList::const_iterator iter;

	positions.clear();
	int i;
	for(i=0,iter=selected_ducks.begin();iter!=selected_ducks.end();++iter,i++)
	{
		Point p((*iter)->get_trans_point());
		positions.push_back(p);
	}

}

void
DuckDrag_Mirror::duck_drag(Duckmatic* duckmatic, const synfig::Vector& vector)
{
	center=vector;
	int i;

	const DuckList selected_ducks(duckmatic->get_selected_ducks());
	DuckList::const_iterator iter;

	Time time(duckmatic->get_time());

	// do the Vertex and Position ducks first
	for(i=0,iter=selected_ducks.begin();iter!=selected_ducks.end();++iter,i++)
		if ((*iter)->get_type() == Duck::TYPE_VERTEX ||
			(*iter)->get_type() == Duck::TYPE_POSITION)
		{
			Vector p(positions[i]);

			if		(axis==AXIS_X) p[0] = -(p[0]-center[0]) + center[0];
			else if	(axis==AXIS_Y) p[1] = -(p[1]-center[1]) + center[1];

			(*iter)->set_trans_point(p, time);
		}

	// then do the other ducks
	for(i=0,iter=selected_ducks.begin();iter!=selected_ducks.end();++iter,i++)
		if ((*iter)->get_type() != Duck::TYPE_VERTEX &&
			(*iter)->get_type() != Duck::TYPE_POSITION)
		{
			// we don't need to mirror radius ducks - they're one-dimensional
			if ((*iter)->is_radius())
				continue;

			Vector p(positions[i]);

			if		(axis==AXIS_X) p[0] = -(p[0]-center[0]) + center[0];
			else if	(axis==AXIS_Y) p[1] = -(p[1]-center[1]) + center[1];

			(*iter)->set_trans_point(p, time);
		}
}

bool
DuckDrag_Mirror::end_duck_drag(Duckmatic* duckmatic)
{
	duckmatic->signal_edited_selected_ducks();
	return true;
}