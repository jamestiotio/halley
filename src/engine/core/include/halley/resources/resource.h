#pragma once

#include <atomic>
#include <thread>
#include <array>
#include <condition_variable>
#include "metadata.h"
#include "halley/concurrency/future.h"
#include "halley/text/enum_names.h"

#if defined(DEV_BUILD) && !defined(__NX_TOOLCHAIN_MAJOR__)
#define ENABLE_HOT_RELOAD
#endif

namespace Halley
{
	enum class ImportAssetType
	{
		Undefined,
		Skip,
		Codegen,
		SimpleCopy,
		Font,
		BitmapFont,
		Image,
		Texture,
		MaterialDefinition,
		Animation,
		ConfigFile,
		AudioClip,
		AudioObject,
		AudioEvent,
		Sprite,
		SpriteSheet,
		Shader,
		Mesh,
		VariableTable,
		GameProperties,
		RenderGraphDefinition,
		ScriptGraph,
		NavmeshSet,
		Prefab,
		Scene,
		UIDefinition
	};

	template <>
	struct EnumNames<ImportAssetType> {
		constexpr std::array<const char*, 26> operator()() const {
			return{{
				"undefined",
				"skip",
				"codegen",
				"simpleCopy",
				"font",
				"bitmapFont",
				"image",
				"texture",
				"materialDefinition",
				"animation",
				"configFile",
				"audioClip",
				"audioObject",
				"audioEvent",
				"sprite",
				"spriteSheet",
				"shader",
				"mesh",
				"variableTable",
				"gameProperties",
				"renderGraphDefinition",
				"scriptGraph",
				"navmeshSet",
				"prefab",
				"scene",
				"uiDefinition"
			}};
		}
	};

	// This order matters.
	// Assets which depend on other types should show up on the list AFTER
	// e.g. since materials depend on shaders, they show after shaders
	enum class AssetType
	{
		BinaryFile,
		TextFile,
		ConfigFile,
		GameProperties,
		Texture,
		Shader,
		MaterialDefinition, // Depends on Texture and Shader
		Image,
		SpriteSheet, // Depends on MaterialDefinition
		Sprite, // Depends on SpriteSheet
		Animation, // Depends on SpriteSheet
		Font, // Depends on SpriteSheet
		AudioClip,
		AudioObject, // Depends on AudioClip
		AudioEvent, // Depends on AudioObject
		Mesh,
		MeshAnimation, // Depends on Mesh
		VariableTable,
		RenderGraphDefinition,
		ScriptGraph,
		NavmeshSet,
		Prefab,
		Scene,
		UIDefinition
	};

	template <>
	struct EnumNames<AssetType> {
		constexpr std::array<const char*, 24> operator()() const {
			return{{
				"binaryFile",
				"textFile",
				"configFile",
				"gameProperties",
				"texture",
				"shader",
				"materialDefinition",
				"image",
				"spriteSheet",
				"sprite",
				"animation",
				"font",
				"audioClip",
				"audioObject",
				"audioEvent",
				"mesh",
				"meshAnimation",
				"variableTable",
				"renderGraphDefinition",
				"scriptGraph",
				"navmeshSet",
				"prefab",
				"scene",
				"uiDefinition"
			}};
		}
	};

	class ResourceObserver;
	class Resources;

	struct ResourceMemoryUsage {
		size_t ramUsage = 0;
		size_t vramUsage = 0;

		ResourceMemoryUsage& operator+=(const ResourceMemoryUsage& other)
		{
			ramUsage += other.ramUsage;
			vramUsage += other.vramUsage;
			return *this;
		}

		String toString() const
		{
			if (vramUsage > 0) {
				return String::prettySize(ramUsage + vramUsage) + " (" + String::prettySize(ramUsage) + " RAM + " + String::prettySize(vramUsage) + " VRAM)";
			} else {
				return String::prettySize(ramUsage);
			}
		}

		size_t getTotal() const
		{
			return ramUsage + vramUsage;
		}
	};

	class Resource
	{
	public:
		virtual ~Resource();

		void setMeta(Metadata meta);
		const Metadata& getMeta() const { return meta; }
		bool isMetaSet() const { return metaSet; }
		
		void setAssetId(String name);
		const String& getAssetId() const { return assetId; }
		virtual void onLoaded(Resources& resources);
		
		int getAssetVersion() const { return assetVersion; }
		void increaseAssetVersion();
		void reloadResource(Resource&& resource);

		virtual ResourceMemoryUsage getMemoryUsage() const;

		void increaseAge(float time);
		void resetAge();
		float getAge() const;

		void setUnloaded();
		bool isUnloaded() const;
		virtual void onOtherResourcesUnloaded();

	protected:
		virtual void reload(Resource&& resource);

	private:
		Metadata meta;
		String assetId;
		int assetVersion = 0;
		float age = 0;
		bool metaSet = false;
		bool unloaded = false;
	};

	class ResourceObserver
	{
	public:
		ResourceObserver();
		ResourceObserver(const Resource& res);
		virtual ~ResourceObserver();

		void startObserving(const Resource& res);
		void stopObserving();
		
		const Resource* getResourceBeingObserved() const;
		bool needsUpdate() const;
		virtual void update();

	private:
		const Resource* res = nullptr;
		int assetVersion = 0;
	};

	class AsyncResource : public Resource
	{
	public:
		AsyncResource();
		virtual ~AsyncResource();

		AsyncResource(const AsyncResource& other);
		AsyncResource(AsyncResource&& other) noexcept;
		AsyncResource& operator=(const AsyncResource& other);
		AsyncResource& operator=(AsyncResource&& other) noexcept;

		void startLoading(); // call from main thread before spinning worker thread
		void doneLoading();  // call from worker thread when done loading
		void loadingFailed(); // Call from worker thread if loading fails
		void waitForLoad(bool acceptFailed = false) const;
		Future<void> onLoad() const;

		bool isLoaded() const;
		bool hasSucceeded() const;
		bool hasFailed() const;

	private:
		std::atomic<bool> failed;
		std::atomic<bool> loading;
		mutable std::condition_variable loadWait;
		mutable std::mutex loadMutex;
		mutable Vector<Promise<void>> pendingPromises;
	};

	struct ResourceOptions {
		bool retainPixelData = false;
		bool retainShaderData = false;
		
		ResourceOptions(bool retainPixelData = false, bool retainShaderData = false)
			: retainPixelData(retainPixelData)
			, retainShaderData(retainShaderData)
		{}
	};
}
