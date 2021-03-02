#pragma once
#include <memory>
#include <vector>

#include "graphics/texture_descriptor.h"

namespace Halley {
	class TextureRenderTarget;
	class Texture;
	class RenderTarget;
	class VideoAPI;
	class RenderGraph;
	class RenderContext;
	class Painter;
	class Material;
	
	class RenderGraphNode {
		friend class RenderGraph;
	
	public:
		using PaintMethod = std::function<void(Painter&)>;
		enum class PinType {
			Unknown,
			ColourBuffer,
			DepthStencilBuffer,
			Texture
		};
		
		void connectInput(uint8_t inputPin, RenderGraphNode& node, uint8_t outputPin);

		void setPaintMethod(PaintMethod paintMethod, std::optional<Colour4f> colourClear, std::optional<float> depthClear, std::optional<uint8_t> stencilClear);
		void setMaterialMethod(std::shared_ptr<Material> material);

	private:
		struct OtherPin {
			RenderGraphNode* node = nullptr;
			uint8_t otherId = 0;
		};
		
		struct InputPin {
			PinType type = PinType::Unknown;
			OtherPin other;
			int8_t textureId = -1;
		};

		struct OutputPin {
			PinType type = PinType::Unknown;
			std::vector<OtherPin> others;
		};
		
		RenderGraphNode(gsl::span<const PinType> inputPins, gsl::span<const PinType> outputPins);

		void startRender();
		void prepareRender(VideoAPI& video, Vector2i targetSize);
		void prepareInputPin(InputPin& pin, VideoAPI& video, Vector2i targetSize);
		void render(const RenderContext& rc, std::vector<RenderGraphNode*>& renderQueue);
		void notifyOutputs(std::vector<RenderGraphNode*>& renderQueue);

		void setRCSinkMethod();
		void resetMethod();

		void resetTextures();
		int8_t makeTexture(VideoAPI& video, PinType type);

		void initializeRenderTarget(VideoAPI& video);
		void prepareRenderTarget();
		
		void renderNode(const RenderContext& rc);
		void renderNodePaintMethod(const RenderContext& rc);
		void renderNodeMaterialMethod(const RenderContext& rc);
		RenderContext getTargetRenderContext(const RenderContext& rc) const;

		std::shared_ptr<Texture> getInputTexture(const InputPin& input);
		std::shared_ptr<Texture> getOutputTexture(uint8_t pin);

		PaintMethod paintMethod;
		std::shared_ptr<Material> materialMethod;
		
		std::optional<Colour4f> colourClear;
		std::optional<float> depthClear;
		std::optional<uint8_t> stencilClear;
		
		bool activeInCurrentPass = false;
		bool rcSinkMethod = false;
		int depsLeft = 0;
		Vector2i currentSize;

		std::vector<InputPin> inputPins;
		std::vector<OutputPin> outputPins;

		std::shared_ptr<TextureRenderTarget> renderTarget;
		std::vector<std::shared_ptr<Texture>> textures;
	};

	class RenderGraph {
	public:
		RenderGraph();
		
		RenderGraphNode& addNode(gsl::span<const RenderGraphNode::PinType> inputPins, gsl::span<const RenderGraphNode::PinType> outputPins);
		RenderGraphNode& getRenderContextNode();

		void render(const RenderContext& rc, VideoAPI& video);

	private:
		std::vector<std::unique_ptr<RenderGraphNode>> nodes;
	};
}
