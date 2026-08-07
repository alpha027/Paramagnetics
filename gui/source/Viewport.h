#ifndef VIEWER_VIEWPORT_H
#define VIEWER_VIEWPORT_H

#include "GeometryBuilder.h"
#include "ViewSettings.h"

#include <greeter/view/Snapshot.h>

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

#include <memory>


namespace viewer {

/*
  The picture.

  Holds a snapshot and draws it. It knows nothing about where the snapshot
  came from, so the same widget shows a run happening now and a run opened
  from a file.
*/
class Viewport: public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {

  Q_OBJECT

  public:

    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    void setSnapshot(const greeter::view::Snapshot& snapshot);

    void setSettings(const ViewSettings& settings);

    const ViewSettings& getSettings() const { return settings; }

    /* Puts the camera where the whole scene is in view. */
    void frameScene();

    /* The range the colours presently cover, for the legend. */
    const ColorRange& getRange() const { return range; }

  signals:

    /* A magnet was clicked, or 0 when the click landed on nothing. */
    void magnetPicked(qint64 id);

  protected:

    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:

    /* One vertex array and its buffer, for a stream of vertices. */
    struct Drawable {
      QOpenGLVertexArrayObject array;
      QOpenGLBuffer buffer{QOpenGLBuffer::VertexBuffer};
      int vertices = 0;
    };

    void rebuildGeometry();
    void uploadGeometry();

    void upload(Drawable& drawable, const std::vector<float>& data,
                const int& floats_per_vertex, const bool& with_normal);

    void drawSurfaces(Drawable& drawable, const bool& lit, const float& alpha);
    void drawLines(Drawable& drawable, const float& width);

    void drawLegend(QPainter& painter);
    void drawHint(QPainter& painter);

    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;

    QVector3D getEye() const;

    /* Where a click points, in the scene. */
    void getRay(const QPoint& at, QVector3D& origin, QVector3D& direction) const;

    greeter::view::Snapshot snapshot;

    ViewSettings settings;

    ColorRange range;

    SurfaceBuffer magnet_surfaces;
    SurfaceBuffer slice_surfaces;
    LineBuffer magnet_outlines;
    LineBuffer magnetization_arrows;
    LineBuffer glyph_lines;
    LineBuffer streamline_lines;
    LineBuffer force_lines;
    LineBuffer box_lines;
    LineBuffer axis_lines;

    std::vector<PickSphere> picks;

    Drawable magnets_drawable;
    Drawable slice_drawable;
    Drawable outlines_drawable;
    Drawable magnetization_drawable;
    Drawable glyphs_drawable;
    Drawable streamlines_drawable;
    Drawable forces_drawable;
    Drawable box_drawable;
    Drawable axes_drawable;

    std::unique_ptr<QOpenGLShaderProgram> surface_program;
    std::unique_ptr<QOpenGLShaderProgram> line_program;

    bool geometry_dirty = true;
    bool ready = false;

    /* The camera: a point it looks at, and where it sits around that. */
    QVector3D target;
    float distance = 1.0f;
    float azimuth = 0.7f;
    float elevation = 0.5f;

    /* How big the scene is, which sets the near and far planes. */
    float scene_scale = 1.0f;

    QPoint last_mouse;
    QPoint press_at;
    Qt::MouseButton pressed = Qt::NoButton;
};

}  // namespace viewer

#endif  // VIEWER_VIEWPORT_H
