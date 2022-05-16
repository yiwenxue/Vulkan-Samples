#pragma once

#include "define.h"
#include "model.h"
#include "scene.h"

#include "common/logging.h"

namespace hair_system
{

class Renderer;
class RenderPipeline;

class RenderPipeline
{
  public:
	RenderPipeline(vkb::Device *device, Renderer *renderer) :
	    _device{device}, _renderer{renderer}
	{}

	~RenderPipeline()
	{
	}

	vkb::Device *get_device()
	{
		return _device;
	}

	Renderer *get_renderer()
	{
		return _renderer;
	}

	virtual void prepare_texture()        = 0;
	virtual void prepare_uniform_buffer() = 0;

	virtual void prepare_descriptor_set_layout() = 0;
	virtual void prepare_descriptor_set()        = 0;
	virtual void prepare_pipeline()              = 0;

	virtual void prepare()
	{
		prepare_texture();

		prepare_uniform_buffer();

		prepare_descriptor_set_layout();

		prepare_descriptor_set();

		prepare_pipeline();
	}

	virtual void destroy()
	{
		if (get_device())
		{
			vkDestroyPipeline(get_device()->get_handle(), pipeline, nullptr);
			vkDestroyPipelineLayout(get_device()->get_handle(), pipeline_layout, nullptr);
			vkDestroyDescriptorSetLayout(get_device()->get_handle(), descriptor_set_layout, nullptr);
		}
	};

  protected:
	// local ds
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;

	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline;

  private:
	vkb::Device *_device;
	Renderer    *_renderer;
};

class GeomPipeline : public RenderPipeline
{
  public:

	GeomPipeline(vkb::Device *device, Renderer *renderer) :
	    RenderPipeline(device, renderer)
	{
		prepare();
	}

	~GeomPipeline()
	{
		destroy();
	}

	static std::vector<VkDescriptorPoolSize> regist_pool_size();

	void prepare_texture() override;

	void prepare_pipeline() override;

	void prepare_uniform_buffer() override;

	void prepare_descriptor_set_layout() override;

	void prepare_descriptor_set() override;

	void destroy() override;

	void draw(VkCommandBuffer cmd, Model *model);

  protected:
};

class HairPipeline : public RenderPipeline
{
  public:
	struct LocalUBO
	{
		glm::mat4 model;
	};

	HairPipeline(vkb::Device *device, Renderer *renderer) :
	    RenderPipeline(device, renderer)
	{
		prepare();
	}

	~HairPipeline()
	{
		destroy();
	}

	static std::vector<VkDescriptorPoolSize> regist_pool_size();

	void prepare_texture() override;

	void prepare_pipeline() override;

	void prepare_uniform_buffer() override;

	void prepare_descriptor_set_layout() override;

	void prepare_descriptor_set() override;

	void destroy() override;

	void draw(VkCommandBuffer cmd, Hair *model);

  protected:
	LocalUBO                           _local_ubo;
	std::unique_ptr<vkb::core::Buffer> _local_buffer;
	Texture                            _texture;
};

class GridStructPipeline : public RenderPipeline
{
  public:
	struct LocalUBO
	{
		glm::mat4 model;
	};
	GridStructPipeline(vkb::Device *device, Renderer *renderer) :
	    RenderPipeline(device, renderer)
	{
		prepare();
	}

	~GridStructPipeline()
	{
		destroy();
	}

	static std::vector<VkDescriptorPoolSize> regist_pool_size();

	void prepare_texture() override;

	void prepare_pipeline() override;

	void prepare_uniform_buffer() override;

	void prepare_descriptor_set_layout() override;

	void prepare_descriptor_set() override;

	void destroy() override;

	void draw(VkCommandBuffer cmd, GridStruct *grid);

  protected:
	LocalUBO                           _local_ubo;
	std::unique_ptr<vkb::core::Buffer> _local_buffer;
	Texture                            _texture;
};
class Renderer
{
  public:
	Renderer(vkb::Device *device) :
	    _device(device)
	{
	}

	~Renderer();

	void prepare_uniform_buffers();

	void update_uniform_buffers();

	void prepare_descriptor_set_layout();

	void prepare_descriptor_set();

	void prepare();

	void render(Scene *scene);

	vkb::Device *get_device()
	{
		return _device;
	}

	VkPipelineShaderStageCreateInfo load_shader(const std::string &file, VkShaderStageFlagBits stage);

	void set_descriptor_pool(VkDescriptorPool *descriptor_pool)
	{
		_descriptor_pool = descriptor_pool;
	}

	VkDescriptorPool *get_descriptor_pool()
	{
		return _descriptor_pool;
	}

	void set_render_pass(VkRenderPass *render_pass)
	{
		_render_pass = render_pass;
	}

	VkRenderPass *get_render_pass()
	{
		return _render_pass;
	}

	void set_pipeline_cache(VkPipelineCache *pipeline_cache)
	{
		_pipeline_cache = pipeline_cache;
	}

	VkPipelineCache *get_pipeline_cache()
	{
		return _pipeline_cache;
	}

	VkDescriptorSetLayout get_global_ds_layouts()
	{
		return _global_ds_layout;
	}

	VkDescriptorSet get_descriptor_set()
	{
		return _global_ds;
	}

	GlobalUBO &get_global_ubo()
	{
		return _global_ubo;
	}

  protected:
  private:
	vkb::Device *_device;

	GlobalUBO _global_ubo;

	std::unique_ptr<vkb::core::Buffer> _global_uniform_buffer;

	std::vector<VkShaderModule> _shader_modules;
	VkDescriptorSetLayout       _global_ds_layout;
	VkDescriptorSet             _global_ds;
	VkRenderPass               *_render_pass;
	VkDescriptorPool           *_descriptor_pool;
	VkPipelineCache            *_pipeline_cache;
};
}        // namespace hair_system