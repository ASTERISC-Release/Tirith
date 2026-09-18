import re

def extract_function_pointer_names(struct_definition, obj_str):
    pattern = re.compile(r'''
        # Return type (e.g. int, void, char, struct something)
        ([\w\s\*]+?)            # return type (non-greedy), including pointers/spaces

        \s*\(\s*\*              # open paren, optional spaces, then asterisk for pointer

        (\w+)                   # function pointer name

        \s*\)\s*                # closing paren around pointer name with optional spaces

        \(                      # open paren for function parameters

        ([^)]*)                 # parameters (anything but a closing paren)

        \)                      # closing paren for function parameters
    ''', re.VERBOSE)

    struct_definition = struct_definition.strip()
    struct_definition = re.sub(r'\s+', ' ', struct_definition).strip()
    for index, line in enumerate(struct_definition.strip().split(';')):
        cpp_code = ""
        if '*/' in line:
            # if ', /*' in line:
                # print(line)
            ret = pattern.match(line.split('*/')[-1])
            if ret:
                pass
                cpp_code += f"if ({obj_str} && accessesStructField(*F, {obj_str}, {index})) {{\n"
                cpp_code += f'    errs() << "  ==> accesses {ret.group(2)}\\n";\n'
                cpp_code += "}\n"
                print(cpp_code)
        else:
            ret = pattern.match(line)
            if ret:
                pass
                cpp_code += f"if ({obj_str} && accessesStructField(*F, {obj_str}, {index})) {{\n"
                cpp_code += f'    errs() << "  ==> accesses {ret.group(2)}\\n";\n'
                cpp_code += "}\n"
                print(cpp_code)


# Example usage
st_context = r"""
   struct gl_context *ctx;
   struct pipe_screen *screen;
   struct pipe_context *pipe;
   struct cso_context *cso_context;

   /* The list of state update functions. */
   st_update_func_t update_functions[ST_NUM_ATOMS];

   struct pipe_frontend_screen *frontend_screen; /* e.g. dri_screen */
   void *frontend_context; /* e.g. dri_context */

   struct draw_context *draw;  /**< For selection/feedback/rastpos only */
   struct draw_stage *feedback_stage;  /**< For GL_FEEDBACK rendermode */
   struct draw_stage *selection_stage;  /**< For GL_SELECT rendermode */
   struct draw_stage *rastpos_stage;  /**< For glRasterPos */

   unsigned pin_thread_counter; /* for L3 thread pinning on AMD Zen */

   GLboolean clamp_frag_color_in_shader;
   GLboolean clamp_vert_color_in_shader;
   bool has_stencil_export; /**< can do shader stencil export? */
   bool has_time_elapsed;
   bool has_etc1;
   bool has_etc2;
   bool transcode_etc;
   bool transcode_astc;
   bool has_astc_2d_ldr;
   bool has_astc_5x5_ldr;
   bool astc_void_extents_need_denorm_flush;
   bool has_s3tc;
   bool has_rgtc;
   bool has_latc;
   bool has_bptc;
   bool prefer_blit_based_texture_transfer;
   bool allow_compute_based_texture_transfer;
   bool force_compute_based_texture_transfer;
   bool force_specialized_compute_transfer;
   bool force_persample_in_shader;
   bool has_shareable_shaders;
   bool has_multi_draw_indirect;
   bool has_indirect_partial_stride;
   bool has_occlusion_query;
   bool has_single_pipe_stat;
   bool has_pipeline_stat;
   bool has_indep_blend_enable;
   bool has_indep_blend_func;
   bool can_dither;
   bool can_bind_const_buffer_as_vertex;
   bool lower_flatshade;
   bool lower_alpha_test;
   bool lower_point_size;
   bool add_point_size;
   bool lower_two_sided_color;
   bool lower_ucp;
   bool prefer_real_buffer_in_constbuf0;
   bool has_conditional_render;
   bool lower_rect_tex;

   /* There are consequences for drivers wanting to call st_finalize_nir
    * twice, once before shader caching and once after lowering for shader
    * variants. If shader variants use lowering passes that are not ready
    * for that, things can blow up.
    *
    * If this is true, st_finalize_nir and pipe_screen::finalize_nir will be
    * called before the result is stored in the shader cache. If lowering for
    * shader variants is invoked, the functions will be called again.
    */
   bool allow_st_finalize_nir_twice;

   /**
    * If a shader can be created when we get its source.
    * This means it has only 1 variant, not counting glBitmap and
    * glDrawPixels.
    */
   bool shader_has_one_variant[MESA_SHADER_STAGES];

   bool needs_texcoord_semantic;
   bool apply_texture_swizzle_to_border_color;
   bool use_format_with_border_color;
   bool alpha_border_color_is_not_w;
   bool emulate_gl_clamp;

   bool draw_needs_minmax_index;
   bool has_hw_atomics;

   bool validate_all_dirty_states;
   bool can_null_texture;

   /* driver supports scissored clears */
   bool can_scissor_clear;

   /* Some state is contained in constant objects.
    * Other state is just parameter values.
    */
   struct {
      struct pipe_blend_state               blend;
      struct pipe_depth_stencil_alpha_state depth_stencil;
      struct pipe_rasterizer_state          rasterizer;
      struct pipe_sampler_state vert_samplers[PIPE_MAX_SAMPLERS];
      struct pipe_sampler_state frag_samplers[PIPE_MAX_SAMPLERS];
      GLuint num_vert_samplers;
      GLuint num_frag_samplers;
      GLuint num_sampler_views[PIPE_SHADER_TYPES];
      unsigned num_images[PIPE_SHADER_TYPES];
      struct pipe_clip_state clip;
      unsigned constbuf0_enabled_shader_mask;
      unsigned fb_width;
      unsigned fb_height;
      unsigned fb_num_samples;
      unsigned fb_num_layers;
      unsigned fb_num_cb;
      unsigned num_viewports;
      struct pipe_scissor_state scissor[PIPE_MAX_VIEWPORTS];
      struct pipe_viewport_state viewport[PIPE_MAX_VIEWPORTS];
      struct {
         unsigned num;
         bool include;
         struct pipe_scissor_state rects[PIPE_MAX_WINDOW_RECTANGLES];
      } window_rects;

      GLuint poly_stipple[32];  /**< In OpenGL's bottom-to-top order */

      GLuint fb_orientation;

      bool enable_sample_locations;
      unsigned sample_locations_samples;
      uint8_t sample_locations[
         PIPE_MAX_SAMPLE_LOCATION_GRID_SIZE *
         PIPE_MAX_SAMPLE_LOCATION_GRID_SIZE * 32];
   } state;

   /** This masks out unused shader resources. Only valid in draw calls. */
   uint64_t active_states;

   /**
    * The number of currently active queries (excluding timer queries).
    * This is used to know if we need to pause any queries for meta ops.
    */
   unsigned active_queries;

   union {
      struct {
         struct gl_program *vp;    /**< Currently bound vertex program */
         struct gl_program *tcp; /**< Currently bound tess control program */
         struct gl_program *tep; /**< Currently bound tess eval program */
         struct gl_program *gp;  /**< Currently bound geometry program */
         struct gl_program *fp;  /**< Currently bound fragment program */
         struct gl_program *cp;   /**< Currently bound compute program */
      };
      struct gl_program *current_program[MESA_SHADER_STAGES];
   };

   struct st_common_variant *vp_variant;

   struct {
      struct pipe_resource *pixelmap_texture;
      struct pipe_sampler_view *pixelmap_sampler_view;
   } pixel_xfer;

   /** for glBitmap */
   struct {
      struct pipe_rasterizer_state rasterizer;
      struct pipe_sampler_state sampler;
      enum pipe_format tex_format;
      struct st_bitmap_cache cache;
   } bitmap;

   /** for glDraw/CopyPixels */
   struct {
      void *zs_shaders[6];
   } drawpix;

   /** Cache of glDrawPixels images */
   struct {
      struct drawpix_cache_entry entries[NUM_DRAWPIX_CACHE_ENTRIES];
      unsigned age;
   } drawpix_cache;

   /** for glReadPixels */
   struct {
      struct pipe_resource *src;
      struct pipe_resource *cache;
      enum pipe_format dst_format;
      unsigned level;
      unsigned layer;
      unsigned hits;
   } readpix_cache;

   /** for glClear */
   struct {
      struct pipe_rasterizer_state raster;
      struct pipe_viewport_state viewport;
      void *vs;
      void *fs;
      void *vs_layered;
      void *gs_layered;
   } clear;

   /* For gl(Compressed)Tex(Sub)Image */
   struct {
      struct pipe_rasterizer_state raster;
      struct pipe_blend_state upload_blend;
      void *vs;
      void *gs;
      void *upload_fs[5][2];
      /**
       * For drivers supporting formatless storing
       * (pipe_caps.image_store_formatted) it is a pointer to the download FS
       * for those not supporting it, it is a pointer to an array of
       * PIPE_FORMAT_COUNT elements, where each element is a pointer to the
       * download FS using that PIPE_FORMAT as the storing format.
       */
      void *download_fs[5][PIPE_MAX_TEXTURE_TYPES][2];
      struct hash_table *shaders;
      bool upload_enabled;
      bool download_enabled;
      bool rgba_only;
      bool layers;
      bool use_gs;
   } pbo;

   struct {
      struct gl_program **progs;
      struct pipe_resource *bc1_endpoint_buf;
      struct pipe_sampler_view *astc_luts[5];
      struct hash_table *astc_partition_tables;
   } texcompress_compute;

   /** for drawing with st_util_vertex */
   struct cso_velems_state util_velems;

   /** passthrough vertex shader matching the util_velem attributes */
   void *passthrough_vs;

   enum pipe_texture_target internal_target;

   void *winsys_drawable_handle;

   bool uses_user_vertex_buffers;

   unsigned last_used_atomic_bindings[PIPE_SHADER_TYPES];
   unsigned last_num_ssbos[PIPE_SHADER_TYPES];

   int32_t draw_stamp;
   int32_t read_stamp;

   struct st_config_options options;

   enum pipe_reset_status reset_status;

   /* Array of bound texture/image handles which are resident in the context.
    */
   struct st_bound_handles bound_texture_handles[PIPE_SHADER_TYPES];
   struct st_bound_handles bound_image_handles[PIPE_SHADER_TYPES];

   /* Winsys buffers */
   struct list_head winsys_buffers;

   /* Throttling for texture uploads and similar operations to limit memory
    * usage by limiting the number of in-flight operations based on
    * the estimated allocated size needed to execute those operations.
    */
   struct util_throttle throttle;

   struct {
      struct st_zombie_sampler_view_node list;
      simple_mtx_t mutex;
   } zombie_sampler_views;

   struct {
      struct st_zombie_shader_node list;
      simple_mtx_t mutex;
   } zombie_shaders;

   struct hash_table *hw_select_shaders;
"""
pipe_screen = r"""
int refcnt;
   void *winsys_priv;

   const struct pipe_caps caps;
   const struct pipe_shader_caps shader_caps[PIPE_SHADER_MESH_TYPES];
   const struct pipe_compute_caps compute_caps;

   /**
    * Get the fd associated with the screen
    * The fd returned is considered read-only, and in particular will not
    * be close()d. It must remain valid for as long as the screen exists.
    */
   int (*get_screen_fd)(struct pipe_screen *);

   /**
    * Atomically incremented by drivers to track the number of contexts.
    * If it's 0, it can be assumed that contexts are not tracked.
    * Used by some places to skip locking if num_contexts == 1.
    */
   unsigned num_contexts;

   /**
    * For drivers using u_transfer_helper:
    */
   struct u_transfer_helper *transfer_helper;

   void (*destroy)(struct pipe_screen *);

   const char *(*get_name)(struct pipe_screen *);

   const char *(*get_vendor)(struct pipe_screen *);

   /**
    * Returns the device vendor.
    *
    * The returned value should return the actual device vendor/manufacturer,
    * rather than a potentially generic driver string.
    */
   const char *(*get_device_vendor)(struct pipe_screen *);

   /**
    * Returns the latest OpenCL CTS version passed
    *
    * The returned value should be the git tag used when passing conformance.
    */
   const char *(*get_cl_cts_version)(struct pipe_screen *);

   /**
    * Query an integer-valued capability/parameter/limit for a codec/profile
    * \param param  one of PIPE_VIDEO_CAP_x
    */
   int (*get_video_param)(struct pipe_screen *,
                          enum pipe_video_profile profile,
                          enum pipe_video_entrypoint entrypoint,
                          enum pipe_video_cap param);

   /**
    * Get the sample pixel grid's size. This function requires
    * pipe_caps.programmable_sample_locations to be callable.
    *
    * \param sample_count - total number of samples
    * \param out_width - the width of the pixel grid
    * \param out_height - the height of the pixel grid
    */
   void (*get_sample_pixel_grid)(struct pipe_screen *, unsigned sample_count,
                                 unsigned *out_width, unsigned *out_height);

   /**
    * Query a timestamp in nanoseconds. The returned value should match
    * PIPE_QUERY_TIMESTAMP. This function returns immediately and doesn't
    * wait for rendering to complete (which cannot be achieved with queries).
    */
   uint64_t (*get_timestamp)(struct pipe_screen *);

   /**
    * Return an equivalent canonical format which has the same component sizes
    * and swizzles as the original, and it is supported by the driver. Gallium
    * already does a first canonicalization step (see get_canonical_format()
    * on st_cb_copyimage.c) and it calls this function (if defined) to get an
    * alternative format if the picked is not supported by the driver.
    */
   enum pipe_format (*get_canonical_format)(struct pipe_screen *,
                                            enum pipe_format format);

   /**
    * Create a context.
    *
    * \param screen      pipe screen
    * \param priv        a pointer to set in pipe_context::priv
    * \param flags       a mask of PIPE_CONTEXT_* flags
    */
   struct pipe_context * (*context_create)(struct pipe_screen *screen,
                                           void *priv, unsigned flags);

   /**
    * Check if the given image copy will be faster on compute
    * \param cpu If true, this is checking against CPU fallback,
    *            otherwise the copy will be on GFX
    */
   bool (*is_compute_copy_faster)(struct pipe_screen *,
                                  enum pipe_format src_format,
                                  enum pipe_format dst_format,
                                  unsigned width,
                                  unsigned height,
                                  unsigned depth,
                                  bool cpu);

   /**
    * Check if the given pipe_format is supported as a texture or
    * drawing surface.
    * \param bindings  bitmask of PIPE_BIND_*
    */
   bool (*is_format_supported)(struct pipe_screen *,
                               enum pipe_format format,
                               enum pipe_texture_target target,
                               unsigned sample_count,
                               unsigned storage_sample_count,
                               unsigned bindings);

   /**
    * Check if the given pipe_format is supported as output for this
    * codec/profile.
    * \param profile  profile to check, may also be PIPE_VIDEO_PROFILE_UNKNOWN
    */
   bool (*is_video_format_supported)(struct pipe_screen *,
                                     enum pipe_format format,
                                     enum pipe_video_profile profile,
                                     enum pipe_video_entrypoint entrypoint);

   /**
    * Check if we can actually create the given resource (test the dimension,
    * overall size, etc).  Used to implement proxy textures.
    * \return TRUE if size is OK, FALSE if too large.
    */
   bool (*can_create_resource)(struct pipe_screen *screen,
                               const struct pipe_resource *templat);

   /**
    * Create a new texture object, using the given template info.
    */
   struct pipe_resource * (*resource_create)(struct pipe_screen *,
                                             const struct pipe_resource *templat);

   struct pipe_resource * (*resource_create_drawable)(struct pipe_screen *,
                                                      const struct pipe_resource *tmpl,
                                                      const void *loader_private);

   struct pipe_resource * (*resource_create_front)(struct pipe_screen *,
                                                   const struct pipe_resource *templat,
                                                   const void *map_front_private);

   /**
    * Create a texture from a winsys_handle. The handle is often created in
    * another process by first creating a pipe texture and then calling
    * resource_get_handle.
    *
    * NOTE: in the case of WINSYS_HANDLE_TYPE_FD handles, the caller
    * retains ownership of the FD.  (This is consistent with
    * EGL_EXT_image_dma_buf_import)
    *
    * \param usage  A combination of PIPE_HANDLE_USAGE_* flags.
    */
   struct pipe_resource * (*resource_from_handle)(struct pipe_screen *,
                                                  const struct pipe_resource *templat,
                                                  struct winsys_handle *handle,
                                                  unsigned usage);

   /**
    * Create a resource from user memory. This maps the user memory into
    * the device address space.
    */
   struct pipe_resource * (*resource_from_user_memory)(struct pipe_screen *,
                                                       const struct pipe_resource *t,
                                                       void *user_memory);

   /**
    * Unlike pipe_resource::bind, which describes what gallium frontends want,
    * resources can have much greater capabilities in practice, often implied
    * by the tiling layout or memory placement. This function allows querying
    * whether a capability is supported beyond what was requested by state
    * trackers. It's also useful for querying capabilities of imported
    * resources where the capabilities are unknown at first.
    *
    * Only these flags are allowed:
    * - PIPE_BIND_SCANOUT
    * - PIPE_BIND_CURSOR
    * - PIPE_BIND_LINEAR
    */
   bool (*check_resource_capability)(struct pipe_screen *screen,
                                     struct pipe_resource *resource,
                                     unsigned bind);

   /**
    * Get a winsys_handle from a texture. Some platforms/winsys requires
    * that the texture is created with a special usage flag like
    * DISPLAYTARGET or PRIMARY.
    *
    * The context parameter can optionally be used to flush the resource and
    * the context to make sure the resource is coherent with whatever user
    * will use it. Some drivers may also use the context to convert
    * the resource into a format compatible for sharing. The use case is
    * OpenGL-OpenCL interop. The context parameter is allowed to be NULL.
    *
    * NOTE: for multi-planar resources (which may or may not have the planes
    * chained through the pipe_resource next pointer) the frontend will
    * always call this function with the first resource of the chain. It is
    * the pipe drivers responsibility to walk the resources as needed when
    * called with handle->plane != 0.
    *
    * NOTE: in the case of WINSYS_HANDLE_TYPE_FD handles, the caller
    * takes ownership of the FD.  (This is consistent with
    * EGL_MESA_image_dma_buf_export)
    *
    * \param usage  A combination of PIPE_HANDLE_USAGE_* flags.
    */
   bool (*resource_get_handle)(struct pipe_screen *,
                               struct pipe_context *context,
                               struct pipe_resource *tex,
                               struct winsys_handle *handle,
                               unsigned usage);

   /**
    * Get info for the given pipe resource without the need to get a
    * winsys_handle.
    *
    * The context parameter can optionally be used to flush the resource and
    * the context to make sure the resource is coherent with whatever user
    * will use it. Some drivers may also use the context to convert
    * the resource into a format compatible for sharing. The context parameter
    * is allowed to be NULL.
    */
   bool (*resource_get_param)(struct pipe_screen *screen,
                              struct pipe_context *context,
                              struct pipe_resource *resource,
                              unsigned plane,
                              unsigned layer,
                              unsigned level,
                              enum pipe_resource_param param,
                              unsigned handle_usage,
                              uint64_t *value);

   /**
    * Get stride and offset for the given pipe resource without the need to get
    * a winsys_handle.
    */
   void (*resource_get_info)(struct pipe_screen *screen,
                             struct pipe_resource *resource,
                             unsigned *stride,
                             unsigned *offset);

   /**
    * Mark the resource as changed so derived internal resources will be
    * recreated on next use.
    *
    * This is necessary when reimporting external images that can't be directly
    * used as texture sampler source, to avoid sampling from old copies.
    */
   void (*resource_changed)(struct pipe_screen *, struct pipe_resource *pt);

   void (*resource_destroy)(struct pipe_screen *,
                            struct pipe_resource *pt);


   /**
    * Do any special operations to ensure frontbuffer contents are
    * displayed, eg copy fake frontbuffer.
    * \param winsys_drawable_handle  an opaque handle that the calling context
    *                                gets out-of-band
    * \param nboxes the number of sub regions to flush
    * \param subbox an array of optional sub regions to flush
    */
   void (*flush_frontbuffer)(struct pipe_screen *screen,
                             struct pipe_context *ctx,
                             struct pipe_resource *resource,
                             unsigned level, unsigned layer,
                             void *winsys_drawable_handle,
                             unsigned nboxes,
                             struct pipe_box *subbox);

   /** Set ptr = fence, with reference counting */
   void (*fence_reference)(struct pipe_screen *screen,
                           struct pipe_fence_handle **ptr,
                           struct pipe_fence_handle *fence);

   /**
    * Wait for the fence to finish.
    *
    * If the fence was created with PIPE_FLUSH_DEFERRED, and the context is
    * still unflushed, and the ctx parameter of fence_finish is equal to
    * the context where the fence was created, fence_finish will flush
    * the context prior to waiting for the fence.
    *
    * In all other cases, the ctx parameter has no effect.
    *
    * \param timeout  in nanoseconds (may be OS_TIMEOUT_INFINITE).
    */
   bool (*fence_finish)(struct pipe_screen *screen,
                        struct pipe_context *ctx,
                        struct pipe_fence_handle *fence,
                        uint64_t timeout);

   /**
    * For fences created with PIPE_FLUSH_FENCE_FD (exported fd) or
    * by create_fence_fd() (imported fd), return the native fence fd
    * associated with the fence.  This may return -1 for fences
    * created with PIPE_FLUSH_DEFERRED if the fence command has not
    * been flushed yet.
    */
   int (*fence_get_fd)(struct pipe_screen *screen,
                       struct pipe_fence_handle *fence);

   /**
    * Retrieves the Win32 shared handle from the fence.
    * Note that Windows fences are pretty much all timeline semaphores,
    * so a value is needed to denote the specific point on the timeline.
    */
   void* (*fence_get_win32_handle)(struct pipe_screen *screen,
                                   struct pipe_fence_handle *fence,
                                   uint64_t *fence_value);

   /**
    * Create a fence from an Win32 handle.
    *
    * This is used for importing a foreign/external fence handle.
    *
    * \param fence  if not NULL, an old fence to unref and transfer a
    *    new fence reference to
    * \param handle opaque handle representing the fence object
    * \param type   indicates which fence types backs the handle
    */
   void (*create_fence_win32)(struct pipe_screen *screen,
                              struct pipe_fence_handle **fence,
                              void *handle,
                              const void *name,
                              enum pipe_fd_type type);

   /**
    * Returns a driver-specific query.
    *
    * If \p info is NULL, the number of available queries is returned.
    * Otherwise, the driver query at the specified \p index is returned
    * in \p info. The function returns non-zero on success.
    */
   int (*get_driver_query_info)(struct pipe_screen *screen,
                                unsigned index,
                                struct pipe_driver_query_info *info);

   /**
    * Returns a driver-specific query group.
    *
    * If \p info is NULL, the number of available groups is returned.
    * Otherwise, the driver query group at the specified \p index is returned
    * in \p info. The function returns non-zero on success.
    */
   int (*get_driver_query_group_info)(struct pipe_screen *screen,
                                      unsigned index,
                                      struct pipe_driver_query_group_info *info);

   /**
    * Query information about memory usage.
    */
   void (*query_memory_info)(struct pipe_screen *screen,
                             struct pipe_memory_info *info);

   /**
    * Get IR specific compiler options struct.  For PIPE_SHADER_IR_NIR this
    * returns a 'struct nir_shader_compiler_options'.  Drivers reporting
    * NIR as the preferred IR must implement this.
    */
   const void *(*get_compiler_options)(struct pipe_screen *screen,
                                      enum pipe_shader_ir ir,
                                      enum pipe_shader_type shader);

   /**
    * Returns a pointer to a driver-specific on-disk shader cache. If the
    * driver failed to create the cache or does not support an on-disk shader
    * cache NULL is returned. The callback itself may also be NULL if the
    * driver doesn't support an on-disk shader cache.
    */
   struct disk_cache *(*get_disk_shader_cache)(struct pipe_screen *screen);

   /**
    * Create a new texture object from the given template info, taking
    * format modifiers into account. \p modifiers specifies a list of format
    * modifier tokens, as defined in drm_fourcc.h. The driver then picks the
    * best modifier among these and creates the resource. \p count must
    * contain the size of \p modifiers array.
    *
    * Returns NULL if an entry in \p modifiers is unsupported by the driver,
    * or if only DRM_FORMAT_MOD_INVALID is provided.
    */
   struct pipe_resource * (*resource_create_with_modifiers)(
                           struct pipe_screen *,
                           const struct pipe_resource *templat,
                           const uint64_t *modifiers, int count);

   /**
    * Get supported modifiers for a format.
    * If \p max is 0, the total number of supported modifiers for the supplied
    * format is returned in \p count, with no modification to \p modifiers.
    * Otherwise, \p modifiers is filled with upto \p max supported modifier
    * codes, and \p count with the number of modifiers copied.
    * The \p external_only array is used to return whether the format and
    * modifier combination can only be used with an external texture target.
    */
   void (*query_dmabuf_modifiers)(struct pipe_screen *screen,
                                  enum pipe_format format, int max,
                                  uint64_t *modifiers,
                                  unsigned int *external_only, int *count);

   /**
    * Create a memory object from a winsys handle
    *
    * The underlying memory is most often allocated in by a foregin API.
    * Then the underlying memory object is then exported through interfaces
    * compatible with EXT_external_resources.
    *
    * Note: For WINSYS_HANDLE_TYPE_FD handles, the caller retains ownership
    * of the fd.
    *
    * \param handle  A handle representing the memory object to import
    */
   struct pipe_memory_object *(*memobj_create_from_handle)(struct pipe_screen *screen,
                                                           struct winsys_handle *handle,
                                                           bool dedicated);

   /**
    * Destroy a memory object
    *
    * \param memobj  The memory object to destroy
    */
   void (*memobj_destroy)(struct pipe_screen *screen,
                          struct pipe_memory_object *memobj);

   /**
    * Create a texture from a memory object
    *
    * \param t       texture template
    * \param memobj  The memory object used to back the texture
    */
   struct pipe_resource * (*resource_from_memobj)(struct pipe_screen *screen,
                                                  const struct pipe_resource *t,
                                                  struct pipe_memory_object *memobj,
                                                  uint64_t offset);

   /**
    * Fill @uuid with a unique driver identifier
    *
    * \param uuid    pointer to a memory region of PIPE_UUID_SIZE bytes
    */
   void (*get_driver_uuid)(struct pipe_screen *screen, char *uuid);

   /**
    * Fill @uuid with a unique device identifier
    *
    * \param uuid    pointer to a memory region of PIPE_UUID_SIZE bytes
    */
   void (*get_device_uuid)(struct pipe_screen *screen, char *uuid);

   /**
    * Fill @luid with the locally unique identifier of the context
    * The LUID returned, paired together with the contexts node mask,
    * allows matching the context to an IDXGIAdapter1 object
    *
    * \param luid    pointer to a memory region of PIPE_LUID_SIZE bytes
    */
   void (*get_device_luid)(struct pipe_screen *screen, char *luid);

   /**
    * Return the device node mask identifying the context
    * Together with the contexts LUID, this allows matching
    * the context to an IDXGIAdapter1 object.
    *
    * within a linked device adapter
    */
   uint32_t (*get_device_node_mask)(struct pipe_screen *screen);

   /**
    * Set the maximum number of parallel shader compiler threads.
    */
   void (*set_max_shader_compiler_threads)(struct pipe_screen *screen,
                                           unsigned max_threads);

   /**
    * Return whether parallel shader compilation has finished.
    */
   bool (*is_parallel_shader_compilation_finished)(struct pipe_screen *screen,
                                                   void *shader,
                                                   enum pipe_shader_type shader_type);

   void (*driver_thread_add_job)(struct pipe_screen *screen,
                                 void *job,
                                 struct util_queue_fence *fence,
                                 pipe_driver_thread_func execute,
                                 pipe_driver_thread_func cleanup,
                                 const size_t job_size);

   /**
    * Set the damage region (called when KHR_partial_update() is invoked).
    * This function is passed an array of rectangles encoding the damage area.
    * rects are using the bottom-left origin convention.
    * nrects = 0 means 'reset the damage region'. What 'reset' implies is HW
    * specific. For tile-based renderers, the damage extent is typically set
    * to cover the whole resource with no damage rect (or a 0-size damage
    * rect). This way, the existing resource content is reloaded into the
    * local tile buffer for every tile thus making partial tile update
    * possible. For HW operating in immediate mode, this reset operation is
    * likely to be a NOOP.
    */
   void (*set_damage_region)(struct pipe_screen *screen,
                             struct pipe_resource *resource,
                             unsigned int nrects,
                             const struct pipe_box *rects);

   /**
    * Run driver-specific NIR lowering and optimization passes.
    *
    * gallium frontends should call this before passing shaders to drivers,
    * and ideally also before shader caching.
    *
    * The driver may return a non-NULL string to trigger GLSL link failure
    * and logging of that message in the GLSL linker log.
    */
   char *(*finalize_nir)(struct pipe_screen *screen, struct nir_shader *nir);

   /*Separated memory/resource allocations interfaces for Vulkan */

   /**
    * Create a resource, and retrieve the required size for it but don't
    * allocate any backing memory.
    */
   struct pipe_resource * (*resource_create_unbacked)(struct pipe_screen *,
                                                      const struct pipe_resource *templat,
                                                      uint64_t *size_required);

   /**
    * Allocate backing memory to be bound to resources.
    */
   struct pipe_memory_allocation *(*allocate_memory)(struct pipe_screen *screen,
                                                     uint64_t size);
   /**
    * Free previously allocated backing memory.
    */
   void (*free_memory)(struct pipe_screen *screen,
                       struct pipe_memory_allocation *);

   /**
    * Allocate fd-based memory to be bound to resources.
    */
   struct pipe_memory_allocation *(*allocate_memory_fd)(struct pipe_screen *screen,
                                                        uint64_t size,
                                                        int *fd,
                                                        bool dmabuf);

   /**
    * Import memory from an fd-handle.
    */
   bool (*import_memory_fd)(struct pipe_screen *screen,
                            int fd,
                            struct pipe_memory_allocation **pmem,
                            uint64_t *size,
                            bool dmabuf);

   /**
    * Free previously allocated fd-based memory.
    */
   void (*free_memory_fd)(struct pipe_screen *screen,
                          struct pipe_memory_allocation *pmem);

   /**
    * Bind memory to a resource.
    */
   bool (*resource_bind_backing)(struct pipe_screen *screen,
                                 struct pipe_resource *pt,
                                 struct pipe_memory_allocation *pmem,
                                 uint64_t fd_offset,
                                 uint64_t size,
                                 uint64_t offset);

   /**
    * Map backing memory.
    */
   void *(*map_memory)(struct pipe_screen *screen,
                       struct pipe_memory_allocation *pmem);

   /**
    * Unmap backing memory.
    */
   void (*unmap_memory)(struct pipe_screen *screen,
                        struct pipe_memory_allocation *pmem);

   /**
    * Determine whether the screen supports the specified modifier
    *
    * Query whether the driver supports a \p modifier in combination with
    * \p format.  If \p external_only is not NULL, the value it points to will
    * be set to 0 or a non-zero value to indicate whether the modifier and
    * format combination is supported only with external, or also with non-
    * external texture targets respectively.  The \p external_only parameter is
    * not used when the function returns false.
    *
    * \return true if the format+modifier pair is supported on \p screen, false
    *         otherwise.
    */
   bool (*is_dmabuf_modifier_supported)(struct pipe_screen *screen,
                                        uint64_t modifier, enum pipe_format,
                                        bool *external_only);

   /**
    * Get the number of planes required for a given modifier/format pair.
    *
    * If not NULL, this function returns the number of planes needed to
    * represent \p format in the layout specified by \p modifier, including
    * any driver-specific auxiliary data planes.
    *
    * Must only be called on a modifier supported by the screen for the
    * specified format.
    *
    * If NULL, no auxiliary planes are required for any modifier+format pairs
    * supported by \p screen.  Hence, the plane count can be derived directly
    * from \p format.
    *
    * \return Number of planes needed to store image data in the layout defined
    *         by \p format and \p modifier.
    */
   unsigned int (*get_dmabuf_modifier_planes)(struct pipe_screen *screen,
                                              uint64_t modifier,
                                              enum pipe_format format);

   /**
    * Get supported page sizes for sparse texture.
    *
    * \p size is the array size of \p x, \p y and \p z.
    *
    * \p offset sets an offset into the possible format page size array,
    *  used to pick a specific xyz size combination.
    *
    * \return Number of supported page sizes, 0 means not support.
    */
   int (*get_sparse_texture_virtual_page_size)(struct pipe_screen *screen,
                                               enum pipe_texture_target target,
                                               bool multi_sample,
                                               enum pipe_format format,
                                               unsigned offset, unsigned size,
                                               int *x, int *y, int *z);

   /**
    * Vertex state CSO functions for precomputing vertex and index buffer
    * states for display lists.
    */
   pipe_create_vertex_state_func create_vertex_state;
   pipe_vertex_state_destroy_func vertex_state_destroy;

   /**
    * Update a timeline semaphore value stored within a driver fence object.
    * Future waits and signals will use the new value.
    */
   void (*set_fence_timeline_value)(struct pipe_screen *screen,
                                    struct pipe_fence_handle *fence,
                                    uint64_t value);

   /**
    * Get additional data for interop_query_device_info
    *
    * \p in_data_size is how much data was allocated by the caller
    * \p data is the buffer to fill
    *
    * \return how much data was written
    */
   uint32_t (*interop_query_device_info)(struct pipe_screen *screen,
                                         uint32_t in_data_size,
                                         void *data);

   /**
    * Get additional data for interop_export_object
    *
    * \p in_data_size is how much data was allocated by the caller
    * \p data is the buffer to fill
    * \p need_export_dmabuf can be set to false to prevent
    *    a following call to resource_get_handle, if the private
    *    data contains the exported data
    *
    * \return how much data was written
    */
   uint32_t (*interop_export_object)(struct pipe_screen *screen,
                                     struct pipe_resource *res,
                                     uint32_t in_data_size,
                                     void *data,
                                     bool *need_export_dmabuf);

   /**
    * Get supported compression fixed rates (bits per component) for a format.
    * If \p max is 0, the total number of supported rates for the supplied
    * format is returned in \p count, with no modification to \p rates.
    * Otherwise, \p rates is filled with upto \p max supported compression
    * rates, and \p count with the number of values copied.
    */
   void (*query_compression_rates)(struct pipe_screen *screen,
                                   enum pipe_format format, int max,
                                   uint32_t *rates, int *count);

   /**
    * Get modifiers associated with a given compression fixed rate.
    * If \p rate is PIPE_COMPRESSION_FIXED_RATE_DEFAULT, supported compression
    * modifiers are returned in order of priority.
    * If \p max is 0, the total number of supported modifiers for the supplied
    * compression rate is returned in \p count, with no modification to \p
    * modifiers. Otherwise, \p modifiers is filled with upto \p max supported
    * modifiers, and \p count with the number of values copied.
    */
   void (*query_compression_modifiers)(struct pipe_screen *screen,
                                       enum pipe_format format, uint32_t rate,
                                       int max, uint64_t *modifiers, int *count);

   /**
    * Check if the given \p target buffer is supported as output (or input for
    * encode) for this \p profile and \p entrypoint.
    *
    * If \p format is different from target->buffer_format this function
    * checks if the \p target buffer can be converted to \p format as part
    * of the given operation (eg. encoder accepts RGB input and converts
    * it to YUV).
    *
    * \return true if the buffer is supported for given operation, false
    *         otherwise.
    */
   bool (*is_video_target_buffer_supported)(struct pipe_screen *screen,
                                            enum pipe_format format,
                                            struct pipe_video_buffer *target,
                                            enum pipe_video_profile profile,
                                            enum pipe_video_entrypoint entrypoint);

   /**
    * pipe_screen is inherited by driver's screen but a simple cast to convert
    * from the generic interface to the driver version won't work if dd_pipe
    * is used.
    */
   struct pipe_screen* (*get_driver_pipe_screen)(struct pipe_screen *screen);
"""
pipe_context = r"""
    struct pipe_screen *screen;

   void *priv;  /**< context private data (for DRI for example) */
   void *draw;  /**< private, for draw module (temporary?) */
   struct u_vbuf *vbuf; /**< for cso_context, don't use in drivers */

   /**
    * Stream uploaders created by the driver. All drivers, gallium frontends, and
    * modules should use them.
    *
    * Use u_upload_alloc or u_upload_data as many times as you want.
    * Once you are done, use u_upload_unmap.
    */
   struct u_upload_mgr *stream_uploader; /* everything but shader constants */
   struct u_upload_mgr *const_uploader;  /* shader constants only */

   /**
    * Debug callback set by u_default_set_debug_callback. Frontends should use
    * set_debug_callback in case drivers need to flush compiler queues.
    */
   struct util_debug_callback debug;

   void (*destroy)(struct pipe_context *);

   /**
    * VBO drawing
    */
   /*@{*/
   /**
    * Multi draw.
    *
    * For indirect multi draws, num_draws is 1 and indirect->draw_count
    * is used instead.
    *
    * Caps:
    * - Always supported: Direct multi draws
    * - pipe_caps.multi_draw_indirect: Indirect multi draws
    * - pipe_caps.multi_draw_indirect_params: Indirect draw count
    *
    * Differences against glMultiDraw and glMultiMode:
    * - "info->mode" and "draws->index_bias" are always constant due to the lack
    *   of hardware support and CPU performance concerns. Only start and count
    *   vary.
    * - if "info->increment_draw_id" is false, draw_id doesn't change between
    *   draws
    *
    * Direct multi draws are also generated by u_threaded_context, which looks
    * ahead in gallium command buffers and merges single draws.
    *
    * \param pipe          context
    * \param info          draw info
    * \param drawid_offset offset to add for drawid param of each draw
    * \param indirect      indirect multi draws
    * \param draws         array of (start, count) pairs for direct draws
    * \param num_draws     number of direct draws 1 for indirect multi draws
    */
   pipe_draw_func draw_vbo;

   /**
    * Multi draw for display lists.
    *
    * For more information, see pipe_vertex_state and
    * pipe_draw_vertex_state_info.
    *
    * Explanation of partial_vertex_mask:
    *
    * 1. pipe_vertex_state::input::elements have a monotonic logical index
    *    determined by pipe_vertex_state::input::full_velem_mask, specifically,
    *    the position of the i-th bit set is the logical index of the i-th
    *    vertex element, up to 31.
    *
    * 2. pipe_vertex_state::input::partial_velem_mask is a subset of
    *    full_velem_mask where the bits set determine which vertex elements
    *    should be bound contiguously. The vertex elements corresponding to
    *    the bits not set in partial_velem_mask should be ignored.
    *
    * Those two allow creating pipe_vertex_state that has more vertex
    * attributes than the vertex shader has inputs. The idea is that
    * pipe_vertex_state can be used with any vertex shader that has the same
    * number of inputs and same logical indices or less. This may sound like
    * an overly complicated way to bind a subset of vertex elements, but it
    * actually simplifies everything else:
    *
    * - In st/mesa, full_velem_mask is exactly the mask of enabled vertex
    *   attributes (VERT_ATTRIB_x) in the display list VAO, while
    *   partial_velem_mask is exactly the inputs_read mask of the vertex
    *   shader (also VERT_ATTRIB_x).
    *
    * - In the driver, some bit ops and popcnt is needed to assemble vertex
    *   elements very quickly.
    */
   void (*draw_vertex_state)(struct pipe_context *ctx,
                             struct pipe_vertex_state *state,
                             uint32_t partial_velem_mask,
                             struct pipe_draw_vertex_state_info info,
                             const struct pipe_draw_start_count_bias *draws,
                             unsigned num_draws);
   /*@}*/

   /**
    * Predicate subsequent rendering on occlusion query result
    * \param query  the query predicate, or NULL if no predicate
    * \param condition whether to skip on FALSE or TRUE query results
    * \param mode  one of PIPE_RENDER_COND_x
    */
   void (*render_condition)(struct pipe_context *pipe,
                            struct pipe_query *query,
                            bool condition,
                            enum pipe_render_cond_flag mode);

   /**
    * Predicate subsequent rendering on a value in a buffer
    * \param buffer The buffer to query for the value
    * \param offset Offset in the buffer to query 32-bit
    * \param condition whether to skip on FALSE or TRUE query results
    */
   void (*render_condition_mem)(struct pipe_context *pipe,
                                struct pipe_resource *buffer,
                                uint32_t offset,
                                bool condition);
   /**
    * Query objects
    */
   /*@{*/
   struct pipe_query *(*create_query)(struct pipe_context *pipe,
                                      unsigned query_type,
                                      unsigned index);

   /**
    * Create a query object that queries all given query types simultaneously.
    *
    * This can only be used for those query types for which
    * get_driver_query_info indicates that it must be used. Only one batch
    * query object may be active at a time.
    *
    * There may be additional constraints on which query types can be used
    * together, in particular those that are implied by
    * get_driver_query_group_info.
    *
    * \param num_queries the number of query types
    * \param query_types array of \p num_queries query types
    * \return a query object, or NULL on error.
    */
   struct pipe_query *(*create_batch_query)(struct pipe_context *pipe,
                                            unsigned num_queries,
                                            unsigned *query_types);

   void (*destroy_query)(struct pipe_context *pipe,
                         struct pipe_query *q);

   bool (*begin_query)(struct pipe_context *pipe, struct pipe_query *q);
   bool (*end_query)(struct pipe_context *pipe, struct pipe_query *q);

   /**
    * Get results of a query.
    * \param wait  if true, this query will block until the result is ready
    * \return TRUE if results are ready, FALSE otherwise
    */
   bool (*get_query_result)(struct pipe_context *pipe,
                            struct pipe_query *q,
                            bool wait,
                            union pipe_query_result *result);

   /**
    * Get results of a query, storing into resource. Note that this may not
    * be used with batch queries.
    *
    * \param wait  if true, this query will block until the result is ready
    * \param result_type  the type of the value being stored:
    * \param index  for queries that return multiple pieces of data, which
    *               item of that data to store (e.g. for
    *               PIPE_QUERY_PIPELINE_STATISTICS).
    *               When the index is -1, instead of the value of the query
    *               the driver should instead write a 1 or 0 to the appropriate
    *               location with 1 meaning that the query result is available.
    */
   void (*get_query_result_resource)(struct pipe_context *pipe,
                                     struct pipe_query *q,
                                     enum pipe_query_flags flags,
                                     enum pipe_query_value_type result_type,
                                     int index,
                                     struct pipe_resource *resource,
                                     unsigned offset);

   /**
    * Set whether all current non-driver queries except TIME_ELAPSED are
    * active or paused.
    */
   void (*set_active_query_state)(struct pipe_context *pipe, bool enable);

   /**
    * INTEL Performance Query
    */
   /*@{*/

   unsigned (*init_intel_perf_query_info)(struct pipe_context *pipe);

   void (*get_intel_perf_query_info)(struct pipe_context *pipe,
                                     unsigned query_index,
                                     const char **name,
                                     uint32_t *data_size,
                                     uint32_t *n_counters,
                                     uint32_t *n_active);

   void (*get_intel_perf_query_counter_info)(struct pipe_context *pipe,
                                             unsigned query_index,
                                             unsigned counter_index,
                                             const char **name,
                                             const char **desc,
                                             uint32_t *offset,
                                             uint32_t *data_size,
                                             uint32_t *type_enum,
                                             uint32_t *data_type_enum,
                                             uint64_t *raw_max);

   struct pipe_query *(*new_intel_perf_query_obj)(struct pipe_context *pipe,
                                                 unsigned query_index);

   bool (*begin_intel_perf_query)(struct pipe_context *pipe, struct pipe_query *q);

   void (*end_intel_perf_query)(struct pipe_context *pipe, struct pipe_query *q);

   void (*delete_intel_perf_query)(struct pipe_context *pipe, struct pipe_query *q);

   void (*wait_intel_perf_query)(struct pipe_context *pipe, struct pipe_query *q);

   bool (*is_intel_perf_query_ready)(struct pipe_context *pipe, struct pipe_query *q);

   bool (*get_intel_perf_query_data)(struct pipe_context *pipe,
                                     struct pipe_query *q,
                                     size_t data_size,
                                     uint32_t *data,
                                     uint32_t *bytes_written);

   /*@}*/

   /**
    * \name GLSL shader/program functions.
    */
   /*@{*/
   /**
    * Called when a shader program is linked.
    * \param handles  Array of shader handles attached to this program.
    *                 The size of the array is \c PIPE_SHADER_TYPES, and each
    *                 position contains the corresponding \c pipe_shader_state*
    *                 or \c pipe_compute_state*, or \c NULL.
    *                 E.g. You can retrieve the fragment shader handle with
    *                      \c handles[PIPE_SHADER_FRAGMENT]
    */
   void (*link_shader)(struct pipe_context *, void** handles);
   /*@}*/

   /**
    * State functions (create/bind/destroy state objects)
    */
   /*@{*/
   void * (*create_blend_state)(struct pipe_context *,
                                const struct pipe_blend_state *);
   void   (*bind_blend_state)(struct pipe_context *, void *);
   void   (*delete_blend_state)(struct pipe_context *, void  *);

   void * (*create_sampler_state)(struct pipe_context *,
                                  const struct pipe_sampler_state *);
   void   (*bind_sampler_states)(struct pipe_context *,
                                 enum pipe_shader_type shader,
                                 unsigned start_slot, unsigned num_samplers,
                                 void **samplers);
   void   (*delete_sampler_state)(struct pipe_context *, void *);

   void * (*create_rasterizer_state)(struct pipe_context *,
                                     const struct pipe_rasterizer_state *);
   void   (*bind_rasterizer_state)(struct pipe_context *, void *);
   void   (*delete_rasterizer_state)(struct pipe_context *, void *);

   void * (*create_depth_stencil_alpha_state)(struct pipe_context *,
                                        const struct pipe_depth_stencil_alpha_state *);
   void   (*bind_depth_stencil_alpha_state)(struct pipe_context *, void *);
   void   (*delete_depth_stencil_alpha_state)(struct pipe_context *, void *);

   void * (*create_fs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_fs_state)(struct pipe_context *, void *);
   void   (*delete_fs_state)(struct pipe_context *, void *);

   void * (*create_vs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_vs_state)(struct pipe_context *, void *);
   void   (*delete_vs_state)(struct pipe_context *, void *);

   void * (*create_gs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_gs_state)(struct pipe_context *, void *);
   void   (*delete_gs_state)(struct pipe_context *, void *);

   void * (*create_tcs_state)(struct pipe_context *,
                              const struct pipe_shader_state *);
   void   (*bind_tcs_state)(struct pipe_context *, void *);
   void   (*delete_tcs_state)(struct pipe_context *, void *);

   void * (*create_tes_state)(struct pipe_context *,
                              const struct pipe_shader_state *);
   void   (*bind_tes_state)(struct pipe_context *, void *);
   void   (*delete_tes_state)(struct pipe_context *, void *);

   void * (*create_vertex_elements_state)(struct pipe_context *,
                                          unsigned num_elements,
                                          const struct pipe_vertex_element *);
   /**
    * Bind vertex elements state.
    *
    * Frontends MUST call set_vertex_buffers after bind_vertex_elements_state
    * and before the next draw. This ensures the driver can apply the state
    * change before the next draw. Drivers MAY use this constraint to merge
    * vertex elements and vertex buffers in set_vertex_buffers instead of
    * in draw_vbo.
    */
   void   (*bind_vertex_elements_state)(struct pipe_context *, void *);
   void   (*delete_vertex_elements_state)(struct pipe_context *, void *);

   void * (*create_ts_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_ts_state)(struct pipe_context *, void *);
   void   (*delete_ts_state)(struct pipe_context *, void *);

   void * (*create_ms_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_ms_state)(struct pipe_context *, void *);
   void   (*delete_ms_state)(struct pipe_context *, void *);
   /*@}*/

   /**
    * Parameter-like state (or properties)
    */
   /*@{*/
   void (*set_blend_color)(struct pipe_context *,
                           const struct pipe_blend_color *);

   void (*set_stencil_ref)(struct pipe_context *,
                           const struct pipe_stencil_ref ref);

   void (*set_sample_mask)(struct pipe_context *,
                           unsigned sample_mask);

   void (*set_min_samples)(struct pipe_context *,
                           unsigned min_samples);

   void (*set_clip_state)(struct pipe_context *,
                          const struct pipe_clip_state *);

   /**
    * Set constant buffer
    *
    * \param shader           Shader stage
    * \param index            Buffer binding slot index within a shader stage
    * \param take_ownership   The callee takes ownership of the buffer reference.
    *                         (the callee shouldn't increment the ref count)
    * \param buf              Constant buffer parameters
    */
   void (*set_constant_buffer)(struct pipe_context *,
                               enum pipe_shader_type shader, uint index,
                               bool take_ownership,
                               const struct pipe_constant_buffer *buf);

   /**
    * Set inlinable constants for constant buffer 0.
    *
    * These are constants that the driver would like to inline in the IR
    * of the current shader and recompile it. Drivers can determine which
    * constants they prefer to inline in finalize_nir and store that
    * information in shader_info::*inlinable_uniform*. When the state tracker
    * or frontend uploads constants to a constant buffer, it can pass
    * inlinable constants separately via this call.
    *
    * Any set_constant_buffer call invalidates this state, so this function
    * must be called after it. Binding a shader also invalidates this state.
    *
    * There is no PIPE_CAP for this. Drivers shouldn't set the shader_info
    * fields if they don't want this or if they don't implement this.
    */
   void (*set_inlinable_constants)(struct pipe_context *,
                                   enum pipe_shader_type shader,
                                   uint num_values, uint32_t *values);

   void (*set_framebuffer_state)(struct pipe_context *,
                                 const struct pipe_framebuffer_state *);

   /**
    * Set the sample locations used during rasterization. When NULL or sized
    * zero, the default locations are used.
    *
    * Note that get_sample_position() still returns the default locations.
    *
    * The samples are accessed with
    * locations[(pixel_y*grid_w+pixel_x)*ms+i],
    * where:
    * ms      = the sample count
    * grid_w  = the pixel grid width for the sample count
    * grid_w  = the pixel grid height for the sample count
    * pixel_x = the window x coordinate modulo grid_w
    * pixel_y = the window y coordinate modulo grid_w
    * i       = the sample index
    * This gives a result with the x coordinate as the low 4 bits and the y
    * coordinate as the high 4 bits. For each coordinate 0 is the left or top
    * edge of the pixel's rectangle and 16 (not 15) is the right or bottom edge.
    *
    * Out of bounds accesses are return undefined values.
    *
    * The pixel grid is used to vary sample locations across pixels and its
    * size can be queried with get_sample_pixel_grid().
    */
   void (*set_sample_locations)(struct pipe_context *,
                                size_t size, const uint8_t *locations);

   void (*set_polygon_stipple)(struct pipe_context *,
                               const struct pipe_poly_stipple *);

   void (*set_scissor_states)(struct pipe_context *,
                              unsigned start_slot,
                              unsigned num_scissors,
                              const struct pipe_scissor_state *);

   void (*set_window_rectangles)(struct pipe_context *,
                                 bool include,
                                 unsigned num_rectangles,
                                 const struct pipe_scissor_state *);

   void (*set_viewport_states)(struct pipe_context *,
                               unsigned start_slot,
                               unsigned num_viewports,
                               const struct pipe_viewport_state *);

   void (*set_sampler_views)(struct pipe_context *,
                             enum pipe_shader_type shader,
                             unsigned start_slot, unsigned num_views,
                             unsigned unbind_num_trailing_slots,
                             struct pipe_sampler_view **views);

   void (*set_tess_state)(struct pipe_context *,
                          const float default_outer_level[4],
                          const float default_inner_level[2]);

   /**
    * Set the number of vertices per input patch for tessellation.
    */
   void (*set_patch_vertices)(struct pipe_context *ctx, uint8_t patch_vertices);

   /**
    * Sets the debug callback. If the pointer is null, then no callback is
    * set, otherwise a copy of the data should be made.
    */
   void (*set_debug_callback)(struct pipe_context *,
                              const struct util_debug_callback *);

   /**
    * Bind an array of shader buffers that will be used by a shader.
    * Any buffers that were previously bound to the specified range
    * will be unbound.
    *
    * \param shader     selects shader stage
    * \param start_slot first buffer slot to bind.
    * \param count      number of consecutive buffers to bind.
    * \param buffers    array of pointers to the buffers to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no buffers will
    *                   be bound.
    * \param writable_bitmask  If bit i is not set, buffers[i] will only be
    *                          used with loads. If unsure, set to ~0.
    */
   void (*set_shader_buffers)(struct pipe_context *,
                              enum pipe_shader_type shader,
                              unsigned start_slot, unsigned count,
                              const struct pipe_shader_buffer *buffers,
                              unsigned writable_bitmask);

   /**
    * Bind an array of hw atomic buffers for use by all shaders.
    * And buffers that were previously bound to the specified range
    * will be unbound.
    *
    * \param start_slot first buffer slot to bind.
    * \param count      number of consecutive buffers to bind.
    * \param buffers    array of pointers to the buffers to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no buffers will
    *                   be bound.
    */
   void (*set_hw_atomic_buffers)(struct pipe_context *,
                                 unsigned start_slot, unsigned count,
                                 const struct pipe_shader_buffer *buffers);

   /**
    * Bind an array of images that will be used by a shader.
    * Any images that were previously bound to the specified range
    * will be unbound.
    *
    * \param shader     selects shader stage
    * \param start_slot first image slot to bind.
    * \param count      number of consecutive images to bind.
    * \param unbind_num_trailing_slots  number of images to unbind after
    *                                   the bound slot
    * \param buffers    array of the images to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no images will
    *                   be bound.
    */
   void (*set_shader_images)(struct pipe_context *,
                             enum pipe_shader_type shader,
                             unsigned start_slot, unsigned count,
                             unsigned unbind_num_trailing_slots,
                             const struct pipe_image_view *images);

   /**
    * Bind an array of vertex buffers to the specified slots.
    *
    * Unlike other set functions, the caller should always increment
    * the buffer reference counts because the driver should only copy
    * the pipe_resource pointers. This is the same behavior as setting
    * take_ownership = true in other functions.
    *
    * count must be equal to the maximum used vertex buffer index + 1
    * in vertex elements or 0.
    *
    * \param count           number of consecutive vertex buffers to bind.
    * \param buffers         array of the buffers to bind
    */
   void (*set_vertex_buffers)(struct pipe_context *,
                              unsigned count,
                              const struct pipe_vertex_buffer *);

   /*@}*/

   /**
    * Stream output functions.
    */
   /*@{*/

   struct pipe_stream_output_target *(*create_stream_output_target)(
                        struct pipe_context *,
                        struct pipe_resource *,
                        unsigned buffer_offset,
                        unsigned buffer_size);

   void (*stream_output_target_destroy)(struct pipe_context *,
                                        struct pipe_stream_output_target *);

   void (*set_stream_output_targets)(struct pipe_context *,
                                     unsigned num_targets,
                                     struct pipe_stream_output_target **targets,
                                     const unsigned *offsets,
                                     enum mesa_prim output_prim);

   uint32_t (*stream_output_target_offset)(struct pipe_stream_output_target *target);

   /*@}*/


   /**
    * INTEL_blackhole_render
    */
   /*@{*/

   void (*set_frontend_noop)(struct pipe_context *,
                             bool enable);

   /*@}*/


   /**
    * Resource functions for blit-like functionality
    *
    * If a driver supports multisampling, blit must implement color resolve.
    */
   /*@{*/

   /**
    * Copy a block of pixels from one resource to another.
    * The resource must be of the same format.
    * Resources with nr_samples > 1 are not allowed.
    */
   void (*resource_copy_region)(struct pipe_context *pipe,
                                struct pipe_resource *dst,
                                unsigned dst_level,
                                unsigned dstx, unsigned dsty, unsigned dstz,
                                struct pipe_resource *src,
                                unsigned src_level,
                                const struct pipe_box *src_box);

   /* Optimal hardware path for blitting pixels.
    * Scaling, format conversion, up- and downsampling (resolve) are allowed.
    */
   void (*blit)(struct pipe_context *pipe,
                const struct pipe_blit_info *info);

   /*@}*/

   /**
    * Clear the specified set of currently bound buffers to specified values.
    * The entire buffers are cleared (no scissor, no colormask, etc).
    *
    * \param buffers  bitfield of PIPE_CLEAR_* values.
    * \param scissor_state  the scissored region to clear
    * \param color  pointer to a union of fiu array for each of r, g, b, a.
    * \param depth  depth clear value in [0,1].
    * \param stencil  stencil clear value
    */
   void (*clear)(struct pipe_context *pipe,
                 unsigned buffers,
                 const struct pipe_scissor_state *scissor_state,
                 const union pipe_color_union *color,
                 double depth,
                 unsigned stencil);

   /**
    * Clear a color rendertarget surface.
    * \param color  pointer to an union of fiu array for each of r, g, b, a.
    */
   void (*clear_render_target)(struct pipe_context *pipe,
                               struct pipe_surface *dst,
                               const union pipe_color_union *color,
                               unsigned dstx, unsigned dsty,
                               unsigned width, unsigned height,
                               bool render_condition_enabled);

   /**
    * Clear a depth-stencil surface.
    * \param clear_flags  bitfield of PIPE_CLEAR_DEPTH/STENCIL values.
    * \param depth  depth clear value in [0,1].
    * \param stencil  stencil clear value
    */
   void (*clear_depth_stencil)(struct pipe_context *pipe,
                               struct pipe_surface *dst,
                               unsigned clear_flags,
                               double depth,
                               unsigned stencil,
                               unsigned dstx, unsigned dsty,
                               unsigned width, unsigned height,
                               bool render_condition_enabled);

   /**
    * Clear the texture with the specified texel. Not guaranteed to be a
    * renderable format. Data provided in the resource's format.
    */
   void (*clear_texture)(struct pipe_context *pipe,
                         struct pipe_resource *res,
                         unsigned level,
                         const struct pipe_box *box,
                         const void *data);

   /**
    * Clear a buffer. Runs a memset over the specified region with the element
    * value passed in through clear_value of size clear_value_size.
    */
   void (*clear_buffer)(struct pipe_context *pipe,
                        struct pipe_resource *res,
                        unsigned offset,
                        unsigned size,
                        const void *clear_value,
                        int clear_value_size);

   /**
    * If a depth buffer is rendered with different sample location state than
    * what is current at the time of reading, the values may differ because
    * depth buffer compression can depend the sample locations.
    *
    * This function is a hint to decompress the current depth buffer to avoid
    * such problems.
    */
   void (*evaluate_depth_buffer)(struct pipe_context *pipe);

   /**
    * Flush draw commands.
    *
    * This guarantees that the new fence (if any) will finish in finite time,
    * unless PIPE_FLUSH_DEFERRED is used.
    *
    * Subsequent operations on other contexts of the same screen are guaranteed
    * to execute after the flushed commands, unless PIPE_FLUSH_ASYNC is used.
    *
    * NOTE: use screen->fence_reference() (or equivalent) to transfer
    * new fence ref to **fence, to ensure that previous fence is unref'd
    *
    * \param fence  if not NULL, an old fence to unref and transfer a
    *    new fence reference to
    * \param flags  bitfield of enum pipe_flush_flags values.
    */
   void (*flush)(struct pipe_context *pipe,
                 struct pipe_fence_handle **fence,
                 unsigned flags);

   /**
    * Create a fence from a fd.
    *
    * This is used for importing a foreign/external fence fd.
    *
    * \param fence  if not NULL, an old fence to unref and transfer a
    *    new fence reference to
    * \param fd     fd representing the fence object
    * \param type   indicates which fence types backs fd
    */
   void (*create_fence_fd)(struct pipe_context *pipe,
                           struct pipe_fence_handle **fence,
                           int fd,
                           enum pipe_fd_type type);

   /**
    * Insert commands to have GPU wait for fence to be signaled.
    */
   void (*fence_server_sync)(struct pipe_context *pipe,
                             struct pipe_fence_handle *fence);

   /**
    * Insert commands to have the GPU signal a fence.
    */
   void (*fence_server_signal)(struct pipe_context *pipe,
                               struct pipe_fence_handle *fence);

   /**
    * Create a view on a texture to be used by a shader stage.
    */
   struct pipe_sampler_view * (*create_sampler_view)(struct pipe_context *ctx,
                                                     struct pipe_resource *texture,
                                                     const struct pipe_sampler_view *templat);

   /**
    * Destroy a view on a texture.
    *
    * \param ctx the current context
    * \param view the view to be destroyed
    *
    * \note The current context may not be the context in which the view was
    *       created (view->context). However, the caller must guarantee that
    *       the context which created the view is still alive.
    */
   void (*sampler_view_destroy)(struct pipe_context *ctx,
                                struct pipe_sampler_view *view);

   /**
    * Signal the driver that the frontend has released a view on a texture.
    *
    * \param ctx the current context
    * \param view the view to be released
    *
    * \note The current context may not be the context in which the view was
    *       created (view->context). Following this call, the driver has full
    *       ownership of the view.
    */
   void (*sampler_view_release)(struct pipe_context *ctx,
                                struct pipe_sampler_view *view);


   /**
    * Get a surface which is a "view" into a resource, used by
    * render target / depth stencil stages.
    */
   struct pipe_surface *(*create_surface)(struct pipe_context *ctx,
                                          struct pipe_resource *resource,
                                          const struct pipe_surface *templat);

   void (*surface_destroy)(struct pipe_context *ctx,
                           struct pipe_surface *);


   /**
    * Map a resource.
    *
    * Transfers are (by default) context-private and allow uploads to be
    * interleaved with rendering.
    *
    * out_transfer will contain the transfer object that must be passed
    * to all the other transfer functions. It also contains useful
    * information (like texture strides for texture_map).
    */
   void *(*buffer_map)(struct pipe_context *,
                       struct pipe_resource *resource,
                       unsigned level,
                       unsigned usage,
                       const struct pipe_box *,
                       struct pipe_transfer **out_transfer);

   /* If transfer was created with WRITE|FLUSH_EXPLICIT, only the
    * regions specified with this call are guaranteed to be written to
    * the resource.
    */
   void (*transfer_flush_region)(struct pipe_context *,
                                 struct pipe_transfer *transfer,
                                 const struct pipe_box *);

   void (*buffer_unmap)(struct pipe_context *,
                        struct pipe_transfer *transfer);

   void *(*texture_map)(struct pipe_context *,
                        struct pipe_resource *resource,
                        unsigned level,
                        unsigned usage,
                        const struct pipe_box *,
                        struct pipe_transfer **out_transfer);

   void (*texture_unmap)(struct pipe_context *,
                         struct pipe_transfer *transfer);

   /* One-shot transfer operation with data supplied in a user
    * pointer.
    */
   void (*buffer_subdata)(struct pipe_context *,
                          struct pipe_resource *,
                          unsigned usage,
                          unsigned offset,
                          unsigned size,
                          const void *data);

   void (*texture_subdata)(struct pipe_context *,
                           struct pipe_resource *,
                           unsigned level,
                           unsigned usage,
                           const struct pipe_box *,
                           const void *data,
                           unsigned stride,
                           uintptr_t layer_stride);

   /**
    * Flush any pending framebuffer writes and invalidate texture caches.
    */
   void (*texture_barrier)(struct pipe_context *, unsigned flags);

   /**
    * Flush caches according to flags.
    */
   void (*memory_barrier)(struct pipe_context *, unsigned flags);

   /**
    * Change the commitment status of a part of the given resource, which must
    * have been created with the PIPE_RESOURCE_FLAG_SPARSE bit.
    *
    * \param level The texture level whose commitment should be changed.
    * \param box The region of the resource whose commitment should be changed.
    * \param commit Whether memory should be committed or un-committed.
    *
    * \return false if out of memory, true on success.
    */
   bool (*resource_commit)(struct pipe_context *, struct pipe_resource *,
                           unsigned level, struct pipe_box *box, bool commit);

   /**
    * Creates a video codec for a specific video format/profile
    */
   struct pipe_video_codec *(*create_video_codec)(struct pipe_context *context,
                                                  const struct pipe_video_codec *templat);

   /**
    * Creates a video buffer as decoding target
    */
   struct pipe_video_buffer *(*create_video_buffer)(struct pipe_context *context,
                                                    const struct pipe_video_buffer *templat);

   /**
    * Compute kernel execution
    */
   /*@{*/
   /**
    * Define the compute program and parameters to be used by
    * pipe_context::launch_grid.
    */
   void *(*create_compute_state)(struct pipe_context *context,
                                 const struct pipe_compute_state *);
   void (*bind_compute_state)(struct pipe_context *, void *);
   void (*delete_compute_state)(struct pipe_context *, void *);

   void (*get_compute_state_info)(struct pipe_context *, void *,
                                  struct pipe_compute_state_object_info *);

   uint32_t (*get_compute_state_subgroup_size)(struct pipe_context *, void *,
                                               const uint32_t block[3]);

   /**
    * Bind an array of buffers to be mapped into the address space of
    * the GLOBAL resource.  Any buffers that were previously bound
    * between [first, first + count - 1] are unbound after this call.
    *
    * \param first      first buffer to map.
    * \param count      number of consecutive buffers to map.
    * \param resources  array of pointers to the buffers to map, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no new
    *                   resources will be bound.
    * \param handles    array of pointers to the memory locations that
    *                   will be updated with the address each buffer
    *                   will be mapped to.  The base memory address of
    *                   each of the buffers will be added to the value
    *                   pointed to by its corresponding handle to form
    *                   the final address argument.  It should contain
    *                   at least \a count elements, unless \a
    *                   resources is NULL in which case \a handles
    *                   should be NULL as well.
    *
    * Note that the driver isn't required to make any guarantees about
    * the contents of the \a handles array being valid anytime except
    * during the subsequent calls to pipe_context::launch_grid.  This
    * means that the only sensible location handles[i] may point to is
    * somewhere within the INPUT buffer itself.  This is so to
    * accommodate implementations that lack virtual memory but
    * nevertheless migrate buffers on the fly, leading to resource
    * base addresses that change on each kernel invocation or are
    * unknown to the pipe driver.
    */
   void (*set_global_binding)(struct pipe_context *context,
                              unsigned first, unsigned count,
                              struct pipe_resource **resources,
                              uint32_t **handles);

   /**
    * Launch the compute kernel starting from instruction \a pc of the
    * currently bound compute program.
    */
   void (*launch_grid)(struct pipe_context *context,
                       const struct pipe_grid_info *info);

   void (*draw_mesh_tasks)(struct pipe_context *context,
                           unsigned drawid_offset,
                           const struct pipe_grid_info *info);
   /*@}*/

   /**
    * SVM (Share Virtual Memory) helpers
    */
   /*@{*/
   /**
    * Migrate range of virtual address to device or host memory.
    *
    * \param to_device - true if the virtual memory is migrated to the device
    *                    false if the virtual memory is migrated to the host
    * \param content_undefined - whether the content of the migrated memory
    *                            is undefined after migration
    */
   void (*svm_migrate)(struct pipe_context *context, unsigned num_ptrs,
                       const void* const* ptrs, const size_t *sizes,
                       bool to_device, bool content_undefined);
   /*@}*/

   /**
    * Get the default sample position for an individual sample point.
    *
    * \param sample_count - total number of samples
    * \param sample_index - sample to get the position values for
    * \param out_value - return value of 2 floats for x and y position for
    *                    requested sample.
    */
   void (*get_sample_position)(struct pipe_context *context,
                               unsigned sample_count,
                               unsigned sample_index,
                               float *out_value);

   /**
    * Query a timestamp in nanoseconds.  This is completely equivalent to
    * pipe_screen::get_timestamp() but takes a context handle for drivers
    * that require a context.
    */
   uint64_t (*get_timestamp)(struct pipe_context *);

   /**
    * Flush the resource cache, so that the resource can be used
    * by an external client. Possible usage:
    * - flushing a resource before presenting it on the screen
    * - flushing a resource if some other process or device wants to use it
    * This shouldn't be used to flush caches if the resource is only managed
    * by a single pipe_screen and is not shared with another process.
    * (i.e. you shouldn't use it to flush caches explicitly if you want to e.g.
    * use the resource for texturing)
    */
   void (*flush_resource)(struct pipe_context *ctx,
                          struct pipe_resource *resource);

   /**
    * Invalidate the contents of the resource. This is used to
    *
    * (1) implement EGL's semantic of undefined depth/stencil
    * contents after a swapbuffers.  This allows a tiled renderer (for
    * example) to not store the depth buffer.
    *
    * (2) implement GL's InvalidateBufferData. For backwards compatibility,
    * you must only rely on the usability for this purpose when
    * pipe_caps.invalidate_buffer is enabled.
    */
   void (*invalidate_resource)(struct pipe_context *ctx,
                               struct pipe_resource *resource);

   /**
    * Return information about unexpected device resets.
    */
   enum pipe_reset_status (*get_device_reset_status)(struct pipe_context *ctx);

   /**
    * Sets the reset status callback. If the pointer is null, then no callback
    * is set, otherwise a copy of the data should be made.
    */
   void (*set_device_reset_callback)(struct pipe_context *ctx,
                                     const struct pipe_device_reset_callback *cb);

   /**
    * Dump driver-specific debug information into a stream. This is
    * used by debugging tools.
    *
    * \param ctx        pipe context
    * \param stream     where the output should be written to
    * \param flags      a mask of PIPE_DUMP_* flags
    */
   void (*dump_debug_state)(struct pipe_context *ctx, FILE *stream,
                            unsigned flags);

   /**
    * Set the log context to which the driver should write internal debug logs
    * (internal states, command streams).
    *
    * The caller must ensure that the log context is destroyed and reset to
    * NULL before the pipe context is destroyed, and that log context functions
    * are only called from the driver thread.
    *
    * \param ctx pipe context
    * \param log logging context
    */
   void (*set_log_context)(struct pipe_context *ctx, struct u_log_context *log);

   /**
    * Emit string marker in cmdstream
    */
   void (*emit_string_marker)(struct pipe_context *ctx,
                              const char *string,
                              int len);

   /**
    * Generate mipmap.
    * \return TRUE if mipmap generation succeeds, FALSE otherwise
    */
   bool (*generate_mipmap)(struct pipe_context *ctx,
                           struct pipe_resource *resource,
                           enum pipe_format format,
                           unsigned base_level,
                           unsigned last_level,
                           unsigned first_layer,
                           unsigned last_layer);

   /**
    * Create a 64-bit texture handle.
    *
    * \param ctx        pipe context
    * \param view       pipe sampler view object
    * \param state      pipe sampler state template
    * \return           a 64-bit texture handle if success, 0 otherwise
    */
   uint64_t (*create_texture_handle)(struct pipe_context *ctx,
                                     struct pipe_sampler_view *view,
                                     const struct pipe_sampler_state *state);

   /**
    * Delete a texture handle.
    *
    * \param ctx        pipe context
    * \param handle     64-bit texture handle
    */
   void (*delete_texture_handle)(struct pipe_context *ctx, uint64_t handle);

   /**
    * Make a texture handle resident.
    *
    * \param ctx        pipe context
    * \param handle     64-bit texture handle
    * \param resident   TRUE for resident, FALSE otherwise
    */
   void (*make_texture_handle_resident)(struct pipe_context *ctx,
                                        uint64_t handle, bool resident);

   /**
    * Create a 64-bit image handle.
    *
    * \param ctx        pipe context
    * \param image      pipe image view template
    * \return           a 64-bit image handle if success, 0 otherwise
    */
   uint64_t (*create_image_handle)(struct pipe_context *ctx,
                                   const struct pipe_image_view *image);

   /**
    * Delete an image handle.
    *
    * \param ctx        pipe context
    * \param handle     64-bit image handle
    */
   void (*delete_image_handle)(struct pipe_context *ctx, uint64_t handle);

   /**
    * Make an image handle resident.
    *
    * \param ctx        pipe context
    * \param handle     64-bit image handle
    * \param access     GL_READ_ONLY, GL_WRITE_ONLY or GL_READ_WRITE
    * \param resident   TRUE for resident, FALSE otherwise
    */
   void (*make_image_handle_resident)(struct pipe_context *ctx, uint64_t handle,
                                      unsigned access, bool resident);

   /**
    * Call the given function from the driver thread.
    *
    * This is set by threaded contexts for use by debugging wrappers.
    *
    * \param asap if true, run the callback immediately if there are no pending
    *             commands to be processed by the driver thread
    */
   void (*callback)(struct pipe_context *ctx, void (*fn)(void *), void *data,
                    bool asap);

   /**
    * Set a context parameter See enum pipe_context_param for more details.
    */
   void (*set_context_param)(struct pipe_context *ctx,
                             enum pipe_context_param param,
                             unsigned value);

   /**
    * Creates a video buffer as decoding target, with modifiers.
    */
   struct pipe_video_buffer *(*create_video_buffer_with_modifiers)(struct pipe_context *context,
                                                                   const struct pipe_video_buffer *templat,
                                                                   const uint64_t *modifiers,
                                                                   unsigned int modifiers_count);

   /**
    * Creates a video buffer as decoding target, from external memory
    */
   struct pipe_video_buffer *(*video_buffer_from_handle)( struct pipe_context *context,
                                                     const struct pipe_video_buffer *templat,
                                                     struct winsys_handle *handle,
                                                     unsigned usage );

   /**
    * Compiles a ML subgraph, to be executed later. The returned pipe_ml_subgraph
    * should contain all information needed to execute the subgraph with as
    * little effort as strictly needed.
    *
    * \param ctx         pipe context
    * \param operations  array containing the definitions of the operations in the graph
    * \param count       number of operations
    * \return            a newly allocated pipe_ml_subgraph
    */
   struct pipe_ml_subgraph *(*ml_subgraph_create)(struct pipe_context *context,
                                                  const struct pipe_ml_operation *operations,
                                                  unsigned count);

   /**
    * Invokes a ML subgraph for a given input tensor.
    *
    * \param ctx         pipe context
    * \param subgraph    previously-compiled subgraph
    * \param inputs_count number of input tensors to copy in
    * \param input_idxs   array with the indices of input tensors
    * \param inputs       array of buffers to copy the tensor data from
    * \param is_signed    per-buffer signed integer flag
    */
   void (*ml_subgraph_invoke)(struct pipe_context *context,
                              struct pipe_ml_subgraph *subgraph,
                              unsigned inputs_count,
                              unsigned input_idxs[],
                              void *inputs[], bool is_signed[]);

   /**
    * After a ML subgraph has been invoked, copy the contents of the output
    * tensors to the provided buffers.
    * 
    * \param ctx           pipe context
    * \param subgraph      previously-executed subgraph
    * \param outputs_count number of output tensors to copy out
    * \param output_idxs   array with the indices of output tensors
    * \param outputs       array of buffers to copy the tensor data to
    * \param is_signed     per-buffer signed integer flag
    */
   void (*ml_subgraph_read_output)(struct pipe_context *context,
                                   struct pipe_ml_subgraph *subgraph,
                                   unsigned outputs_count, unsigned output_idxs[],
                                   void *outputs[], bool is_signed[]);

   /**
    * Release all resources allocated by the implementation of ml_subgraph_create
    * 
    * \param ctx           pipe context
    * \param subgraph      subgraph to release
    */
   void (*ml_subgraph_destroy)(struct pipe_context *context,
                               struct pipe_ml_subgraph *subgraph);
"""

names = extract_function_pointer_names(pipe_context, "pipeConCtxTy")
names = extract_function_pointer_names(pipe_screen, "pipeScrCtxTy")
names = extract_function_pointer_names(st_context, "stCtxTy")

