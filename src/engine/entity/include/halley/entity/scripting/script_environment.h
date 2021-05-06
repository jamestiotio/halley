#pragma once
#include "script_node_type.h"

namespace Halley {
    class ScriptState;
	
    class ScriptEnvironment {
    public:
    	ScriptEnvironment(const HalleyAPI& api, World& world, Resources& resources, const ScriptNodeTypeCollection& nodeTypeCollection);
    	virtual ~ScriptEnvironment() = default;

    	virtual void update(Time time, const ScriptGraph& graph, ScriptState& graphState);

    	EntityRef getEntity(EntityId entityId);
    	const ScriptGraph* getCurrentGraph() const;

    protected:
		const HalleyAPI& api;
    	World& world;
    	Resources& resources;
    	const ScriptNodeTypeCollection& nodeTypeCollection;
    	const ScriptGraph* currentGraph;

    private:
        std::unique_ptr<IScriptStateData> makeNodeData(const IScriptNodeType& nodeType, const ScriptGraphNode& node);
    };
}
