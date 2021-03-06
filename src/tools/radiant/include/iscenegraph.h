/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined (INCLUDED_ISCENEGRAPH_H)
#define INCLUDED_ISCENEGRAPH_H

#include <cstddef>

#include "inode.h"
#include "itraversable.h"

#include "generic/constant.h"
#include "signal/signalfwd.h"

template<typename value_type>
class Stack;
template<typename Contained>
class Reference;

namespace scene
{
	class Instance;
	const Instance* const nullInstancePointer = 0;
	inline const Instance& nullInstance ()
	{
		return *nullInstancePointer;
	}

	class Node;
	const Node* const nullNodePointer = 0;
	inline const Node& nullNode ()
	{
		return *nullNodePointer;
	}
}

typedef Reference<scene::Node> NodeReference;

/**
 * @defgroup scenegraph Scenegraph
 *
 * @namespace scene
 * @ingroup scenegraph
 * Interfaces and types relating to the scene-graph.
 */
namespace scene
{
	/** @brief A unique key to an instance of a node in the scene-graph. */
	typedef Stack<NodeReference> Path;

	class EraseObserver {
		public:
			virtual ~EraseObserver() {};

			virtual void onErase(scene::Instance* instance) = 0;
	};

	/**
	 * @brief A scene-graph - a Directed Acyclic Graph (DAG).
	 *
	 * Each node may refer to zero or more 'child' nodes (directed).
	 * A node may never have itself as one of its ancestors (acyclic).
	 * Each node may have more than one 'parent', thus having more than one
	 * 'instance' in the graph.
	 * Each instance is uniquely identified by the list of its ancestors plus
	 * itself, known as a 'path'.
	 */
	class Graph
	{
		public:
			INTEGER_CONSTANT(Version, 1);
			STRING_CONSTANT(Name, "scenegraph");

			class Walker
			{
				public:
					virtual ~Walker ()
					{
					}
					/** @brief Called before traversing the first child-instance of 'instance'. If the return value is false, the children of the current instance are not traversed. */
					virtual bool pre (const Path& path, Instance& instance) const = 0;
					/** @brief Called after traversing the last child-instance of 'instance'. */
					virtual void post (const Path& path, Instance& instance) const
					{
					}
			};

			/* greebo: Derive from this class to get notified on scene changes
			 */
			class Observer {
			public:
				virtual ~Observer ()
				{
				}

				// Gets called when anything in the scenegraph changes
				virtual void onSceneGraphChange ()
				{
				}

				// Gets called when a new <instance> is inserted into the scenegraph
				virtual void onSceneNodeInsert (const scene::Instance& instance)
				{
				}

				// Gets called when <instance> is removed from the scenegraph
				virtual void onSceneNodeErase (const scene::Instance& instance)
				{
				}
			};

			virtual ~Graph ()
			{
			}

			/** greebo: Adds/removes an observer from the scenegraph,
			 * 			to get notified upon insertions/deletions
			 */
			virtual void addSceneObserver (scene::Graph::Observer* observer) = 0;
			virtual void removeSceneObserver (scene::Graph::Observer* observer) = 0;

			// will inform the observers if a instance was removed
			virtual void notifyErase (scene::Instance* instance) = 0;
			// register a new observer that is notified whenever an instance is removed from the graph
			virtual void addEraseObserver(scene::EraseObserver *) = 0;
			// unregister the given observers again
			virtual void removeEraseObserver(scene::EraseObserver *) = 0;

			/** @brief Returns the root-node of the graph. */
			virtual Node& root () = 0;
			/** @brief Sets the root-node of the graph to be 'node'. */
			virtual void insert_root (Node& root) = 0;
			/** @brief Clears the root-node of the graph. */
			virtual void erase_root () = 0;
			/** @brief Traverses all nodes in the graph depth-first, starting from the root node. */
			virtual void traverse (const Walker& walker) = 0;
			/** @brief Traverses all nodes in the graph depth-first, starting from 'start'. */
			virtual void traverse_subgraph (const Walker& walker, const Path& start) = 0;
			/** @brief Returns the instance at the location identified by 'path', or 0 if it does not exist. */
			virtual scene::Instance* find (const Path& path) = 0;
			/** @brief Returns the instance at the location identified by 'node', or 0 if it does not exist. */
			virtual scene::Instance* find (Node& node) = 0;

			/** @brief Invokes all scene-changed callbacks. Called when any part of the scene changes the way it will appear when the scene is rendered.
			 * @todo Move to a separate class. */
			virtual void sceneChanged () = 0;

			/** @brief Invokes all bounds-changed callbacks. Called when the bounds of any instance in the scene change. */
			/** @todo Move to a separate class. */
			virtual void boundsChanged () = 0;
			/** @brief Add a \p callback to be invoked when the bounds of any instance in the scene change. */
			virtual SignalHandlerId addBoundsChangedCallback (const SignalHandler& boundsChanged) = 0;
			/** @brief Remove a \p callback to be invoked when the bounds of any instance in the scene change. */
			virtual void removeBoundsChangedCallback (SignalHandlerId id) = 0;
	};

	class Instantiable
	{
		public:
			STRING_CONSTANT(Name, "scene::Instantiable");

			class Observer
			{
				public:
					virtual ~Observer ()
					{
					}

					/** @brief Called when an instance is added to the container. */
					virtual void insert (scene::Instance* instance) = 0;
					/** @brief Called when an instance is removed from the container. */
					virtual void erase (scene::Instance* instance) = 0;
			};

			class Visitor
			{
				public:
					virtual ~Visitor ()
					{
					}

					virtual void visit (Instance& instance) const = 0;
			};

			virtual ~Instantiable ()
			{
			}

			/** @brief Returns a new instance uniquely identified by 'path'. */
			virtual scene::Instance* create (const scene::Path& path, scene::Instance* parent) = 0;
			/** @brief Calls Visitor::visit(instance) for each instance in the container. */
			virtual void forEachInstance (const Visitor& visitor) = 0;
			/** @brief Adds an instance to the container. */
			virtual void insert (Observer* observer, const Path& path, scene::Instance* instance) = 0;
			/** @brief Returns an instance removed from the container. */
			virtual scene::Instance* erase (Observer* observer, const Path& path) = 0;
	};

	class Cloneable
	{
		public:
			STRING_CONSTANT(Name, "scene::Cloneable");

			virtual ~Cloneable ()
			{
			}

			/** @brief Returns a copy of itself. */
			virtual scene::Node& clone () const = 0;
	};
}

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<scene::Graph> GlobalSceneGraphModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<scene::Graph> GlobalSceneGraphModuleRef;

inline scene::Graph& GlobalSceneGraph ()
{
	return GlobalSceneGraphModule::getTable();
}

inline void SceneChangeNotify ()
{
	GlobalSceneGraph().sceneChanged();
}

#endif
