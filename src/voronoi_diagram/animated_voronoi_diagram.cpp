#include "animated_voronoi_diagram.h"
#include "GL/glut.h"
#include <cstdio>

namespace voronoi_diagram
{


	void AnimatedVoronoiDiagram::display()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		float diagram_width = VoronoiDiagram::getWidth();
		float diagram_height = VoronoiDiagram::getHeight();
		gluOrtho2D(-view_width_/2.0f, view_width_/2.0f, -view_height_/2.0f, view_height_/2.0f);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glPointSize(4.0f);
		glColor3f(1.0f, 1.0f, 1.0f);
		glBegin(GL_POINTS); {
			SitesConstPtr sites = VoronoiDiagram::sites_;
			for(Sites::const_iterator site = sites->begin();
				site != sites->end();
				site++)
			{
				glVertex2f((*site)->x, (*site)->y);
			}
		} glEnd();

		glColor3f(0.0f, 0.0f, 1.0f);
		glBegin(GL_LINES); {
			glVertex2f(-diagram_width/2.0f, diagram_height/2.0f);
			glVertex2f(diagram_width/2.0f, diagram_height/2.0f);

			glVertex2f(-diagram_width/2.0f, -diagram_height/2.0f);
			glVertex2f(diagram_width/2.0f, -diagram_height/2.0f);

			glVertex2f(-diagram_width/2.0f, diagram_height/2.0f);
			glVertex2f(-diagram_width/2.0f, -diagram_height/2.0f);

			glVertex2f(diagram_width/2.0f, diagram_height/2.0f);
			glVertex2f(diagram_width/2.0f, -diagram_height/2.0f);
		} glEnd();

	

		// draw a red line at the sweepline position
		glColor3f(1.0f, 0.0f, 0.0f);
		glBegin(GL_LINES); {
			if(curr_event_)
			{
				glVertex2f(-view_width_/2.0f, curr_event_->y_);
				glVertex2f(view_width_/2.0f, curr_event_->y_);
			}
		} glEnd();

		// draw the beachline in green
		// TODO: Stop the parabolas at the intersections
		glColor3f(0.0f, 1.0f, 0.0f);
		if(beachline_ && curr_event_)
			drawBeachline(beachline_, curr_event_->y_);

		// Draw the incomplete edges
		if(edges_)
		{
			glColor3f(1.0f, 0.0f, 1.0f);
			glBegin(GL_LINES); {
				for(Edges::iterator edge = edges_->begin();
					edge != edges_->end();
					edge++)
				{
					if(!(*edge)->end_)
						continue;
					glVertex2f((*edge)->start_->x, (*edge)->start_->y);
					glVertex2f((*edge)->end_->x, (*edge)->end_->y);
					if(!(*edge)->neighbor_)
						continue;
					glVertex2f((*edge)->neighbor_->start_->x, (*edge)->neighbor_->start_->y);
					glVertex2f((*edge)->neighbor_->end_->x, (*edge)->neighbor_->end_->y);
				}
			} glEnd();
		}

		glutSwapBuffers();
	}

	void AnimatedVoronoiDiagram::setViewWidth(float width)
	{
		view_width_ = width;
		glutPostRedisplay();
	}

	void AnimatedVoronoiDiagram::setViewHeight(float height)
	{
		view_height_ = height;
		glutPostRedisplay();
	}

	void AnimatedVoronoiDiagram::nextEvent()
	{
		if(finished_animation_)
			return;

		curr_event_.reset();

		while(!curr_event_ && !event_queue_.empty())
		{
			curr_event_ = event_queue_.top();
			event_queue_.pop();
			if(deleted_events_.find(curr_event_) != deleted_events_.end())
			{
				// remove from the set of deleted events
				deleted_events_.erase(curr_event_);
				curr_event_.reset();
				continue;
			}

			if(curr_event_->type_ == EventType::SITE)
			{
				printf("event: (%3.3f, %3.3f)\n", curr_event_->site_->x, curr_event_->site_->y);
				addArc(curr_event_);
			}
			else
			{
				printf("event: (%3.3f, %3.3f)\n", curr_event_->site_->x, curr_event_->site_->y);
				removeArc(curr_event_);
			}
		}

		if(!curr_event_)
		{
			VoronoiDiagram::finishEdges(beachline_, view_width_, view_height_);
			beachline_.reset();
			finished_animation_ = true;
		}
	}

	void AnimatedVoronoiDiagram::restartAnimation()
	{
		finished_animation_ = false;
		edges_.reset(new Edges());
		while(!event_queue_.empty())
			event_queue_.pop();
		beachline_.reset();

		for(Sites::iterator site = sites_->begin();
			site != sites_->end();
			site++)
		{
			EventPtr site_event(new Event(*site));
			event_queue_.push(site_event);
		}
	}

	void AnimatedVoronoiDiagram::drawBeachline(BeachlineNodePtr node, float line_position)
	{
		if(node->type_ != BeachlineNodeType::ARC)
		{
			Point edge_point = VoronoiDiagram::getEdgePoint(node, line_position);

			glColor3f(1.0f, 1.0f, 0.0f);
			glPointSize(4.0f);
			glBegin(GL_POINTS); {
				glVertex2f(edge_point.x, edge_point.y);
			} glEnd();

			node->edge_->end_.reset(new Point(edge_point.x, edge_point.y));

			drawBeachline(node->getLeftChild(), line_position);
			drawBeachline(node->getRightChild(), line_position);
			return;
		}

		const int NUM_ARC_SAMPLES=100;

		BeachlineNodePtr left_edge = BeachlineNode::getLeftEdge(node);
		BeachlineNodePtr right_edge = BeachlineNode::getRightEdge(node);

		Point left_edge_point(-view_width_/2.0f, view_height_/2.0f);
		if(left_edge)
			left_edge_point = VoronoiDiagram::getEdgePoint(left_edge, line_position);

		Point right_edge_point(view_width_/2.0f, view_height_/2.0f);
		if(right_edge)
			right_edge_point = VoronoiDiagram::getEdgePoint(right_edge, line_position);

		float lower_x = left_edge_point.x;
		float upper_x = right_edge_point.x;

		glColor3f(0, 1.0f, 0);
		glBegin(GL_LINE_STRIP); {
			// degenerate case when sweepline is on the point
			// the arc is actually a vertical line
			if(node->site_->y == line_position)
			{
				glVertex2f(node->site_->x, left_edge_point.y);
				glVertex2f(node->site_->x, line_position);
			}
			else
			{
				for(int j=0; j<NUM_ARC_SAMPLES; j++)
				{
					float x = j*((upper_x - lower_x)/NUM_ARC_SAMPLES) + lower_x;
					float a = node->site_->x;
					float b = node->site_->y;
					float l = line_position;

					float y = (a*a - 2.0f*a*x + b*b - l*l + x*x)/(2.0f*(b-l));

					glVertex2f(x, y);
				}
			}
		} glEnd();

	}
}
