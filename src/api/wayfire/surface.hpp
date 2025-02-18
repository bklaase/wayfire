#ifndef WF_SURFACE_HPP
#define WF_SURFACE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include <wayfire/nonstd/wlroots.hpp>
#include <wayfire/nonstd/observer_ptr.h>
#include <wayfire/geometry.hpp>
#include <wayfire/object.hpp>
#include <wayfire/scene.hpp>

namespace wf
{
class output_t;
class surface_interface_t;
struct render_target_t;
struct region_t;

/**
 * A surface and its position on the screen.
 */
struct surface_iterator_t
{
    /** The surface */
    surface_interface_t *surface;
    /**
     * The position of the surface relative to the topmost surface in the
     * surface tree
     */
    wf::point_t position;
};

/**
 * surface_interface_t is the base class for everything that can be displayed
 * on the screen. It is the closest thing there is in Wayfire to a Window in X11.
 */
class surface_interface_t : public wf::object_base_t
{
  public:
    virtual ~surface_interface_t();

    surface_interface_t(const surface_interface_t &) = delete;
    surface_interface_t(surface_interface_t &&) = delete;
    surface_interface_t& operator =(const surface_interface_t&) = delete;
    surface_interface_t& operator =(surface_interface_t&&) = delete;

    /**
     * Check whether the surface is mapped. Mapped surfaces are "alive", i.e
     * they are rendered, can receive input, etc.
     *
     * Note an unmapped surface may be temporarily rendered, for ex. when
     * using a close animation.
     *
     * @return true if the surface is mapped and false otherwise.
     */
    virtual bool is_mapped() const = 0;

    /**
     * Related surfaces usually form hierarchies, where the topmost surface
     * is a view, except for drag icons or surfaces managed by plugins.
     *
     * @return The topmost surface in the hierarchy of the surface.
     */
    virtual surface_interface_t *get_main_surface();

    /**
     * Add a new subsurface to the surface.
     * This may potentially change the subsurface's output.
     *
     * @param subsurface The subsurface to add.
     * @param is_below_parent If true, then the subsurface will be placed below
     *  the view, otherwise, it will be on top of it.
     */
    void add_subsurface(std::unique_ptr<surface_interface_t> subsurface,
        bool is_below_parent);

    /**
     * Remove the given subsurface from the surface tree.
     *
     * No-op if the subsurface does not exist.
     *
     * @param subsurface The subsurface to remove.
     * @return The subsurface removed or nullptr.
     */
    std::unique_ptr<surface_interface_t> remove_subsurface(
        nonstd::observer_ptr<surface_interface_t> subsurface);

    /**
     * @param surface_origin The coordinates of the top-left corner of the
     * surface.
     *
     * @return a list of each mapped surface in the surface tree, including the
     * surface itself.
     *
     * The surfaces should be ordered from the topmost to the bottom-most one.
     */
    virtual std::vector<surface_iterator_t> enumerate_surfaces(
        wf::point_t surface_origin = {0, 0});

    /**
     * @return The output the surface is currently attached to. Note this
     * doesn't necessarily mean that it is visible.
     */
    virtual wf::output_t *get_output();

    /**
     * Set the current output of the surface and all surfaces in its surface
     * tree. Note that calling this for a surface with a parent is an invalid
     * operation and may have undefined consequences.
     *
     * @param output The new output, may be null
     */
    virtual void set_output(wf::output_t *output);

    /** @return The offset of this surface relative to its parent surface.  */
    virtual wf::point_t get_offset() = 0;

    /** @return The surface dimensions */
    virtual wf::dimensions_t get_size() const = 0;

    /**
     * Test whether the surface accepts touch or pointer input at the given
     * surface-local position. By default, the surface doesn't accept any
     * input.
     *
     * @return true if the point lies inside the input region of the surface,
     * false otherwise.
     */
    virtual bool accepts_input(int32_t sx, int32_t sy);

    /**
     * Send wl_surface.frame event. Surfaces which aren't backed by a
     * wlr_surface don't need to do anything here.
     *
     * @param frame_end The time when the frame ended.
     */
    virtual void send_frame_done(const timespec& frame_end);

    /**
     * Get the opaque region of the surface relative to the given point.
     *
     * This is just a hint, so surface implementations don't have to implement
     * this function.
     *
     * @param origin The coordinates of the upper-left corner of the surface.
     */
    virtual wf::region_t get_opaque_region(wf::point_t origin);

    /**
     * @return the wl_client associated with this surface, or null if the
     *   surface doesn't have a backing wlr_surface.
     */
    virtual wl_client *get_client();

    /**
     * @return the wlr_surface associated with this surface, or null if no
     *   the surface doesn't have a backing wlr_surface. */
    virtual wlr_surface *get_wlr_surface();

    /**
     * Render the surface, without applying any transformations.
     *
     * @param fb The target framebuffer.
     * @param x The x coordinate of the surface in the same coordinate system
     *   as the framebuffer's geometry.
     * @param y The y coordinate of the surface the same coordinate system
     *   as the framebuffer's geometry.
     * @param damage The damaged region of the surface, in the same coordinate
     *   system as the framebuffer's geometry. Nothing should be drawn outside
     *   of the damaged region.
     */
    virtual void simple_render(const wf::render_target_t& fb, int x, int y,
        const wf::region_t& damage) = 0;

    /**
     * Get the main node of the surface which contains its content (as opposed
     * to the node which contains content + subsurfaces).
     */
    wf::scene::node_ptr get_content_node() const;

    /** Private data for surface_interface_t, used for the implementation of
     * provided functions */
    class impl;
    std::unique_ptr<impl> priv;

  protected:
    /** Construct a new surface. */
    surface_interface_t();

    /** Damage the given box, in surface-local coordinates */
    virtual void damage_surface_box(const wlr_box& box);
    /** Damage the given region, in surface-local coordinates */
    virtual void damage_surface_region(const wf::region_t& region);

    /** Remove all subsurfaces that we have. Should to be called after unmapping! */
    virtual void clear_subsurfaces();

    /* Allow wlr surface implementation to access surface internals */
    friend class wlr_surface_base_t;
};

namespace scene
{
/**
 * Each surface is represented by two nodes. One of them is a view_node_t,
 * which contains the surface node itself and its children's
 * surface_root_node_t's, for example like this:
 *
 * - view_node_t(main surface):
 *   - surface_root_node_t(subsurface above)
 *     - surface_node_t(subsurface above)
 *   - surface_node_t(main surface)
 *   - surface_root_node_t(subsurface below)
 *     - surface_node_t(subsurface below)
 */
class surface_root_node_t : public wf::scene::floating_inner_node_t
{
  public:
    surface_root_node_t(wf::surface_interface_t *si);

    wf::pointf_t to_local(const wf::pointf_t& point) override;
    wf::pointf_t to_global(const wf::pointf_t& point) override;

    std::string stringify() const override;
    void gen_render_instances(
        std::vector<render_instance_uptr>& instances,
        damage_callback damage,
        wf::output_t *output) override;
    wf::geometry_t get_bounding_box() override;

  private:
    wf::surface_interface_t *si;
};

class surface_node_t : public wf::scene::node_t
{
  public:
    surface_node_t(wf::surface_interface_t *si);

    std::optional<input_node_t> find_node_at(const wf::pointf_t& at) override;

    std::string stringify() const override;
    pointer_interaction_t& pointer_interaction() override;
    touch_interaction_t& touch_interaction() override;

    wf::surface_interface_t *get_surface() const
    {
        return si;
    }

    void gen_render_instances(
        std::vector<render_instance_uptr>& instances,
        damage_callback damage,
        wf::output_t *output) override;
    wf::geometry_t get_bounding_box() override;

  private:
    std::unique_ptr<pointer_interaction_t> ptr_interaction;
    std::unique_ptr<touch_interaction_t> tch_interaction;
    wf::surface_interface_t *si;
};
}

void emit_map_state_change(wf::surface_interface_t *surface);
}

#endif /* end of include guard: WF_SURFACE_HPP */
