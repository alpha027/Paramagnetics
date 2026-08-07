#include "Viewport.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>


namespace {

const char* SURFACE_VERTEX = R"(
#version 330 core
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_color;
uniform mat4 u_transform;
out vec3 v_normal;
out vec3 v_color;
void main() {
  gl_Position = u_transform * vec4(in_position, 1.0);
  v_normal = in_normal;
  v_color = in_color;
}
)";

const char* SURFACE_FRAGMENT = R"(
#version 330 core
in vec3 v_normal;
in vec3 v_color;
uniform vec3 u_light;
uniform float u_lit;
uniform float u_alpha;
out vec4 fragment;
void main() {
  float shade = 1.0;
  if (u_lit > 0.5) {
    vec3 normal = normalize(v_normal);
    // Lit from both sides: the inside of a cut open shape should not be black.
    float facing = abs(dot(normal, normalize(u_light)));
    shade = 0.35 + 0.65 * facing;
  }
  fragment = vec4(v_color * shade, u_alpha);
}
)";

const char* LINE_VERTEX = R"(
#version 330 core
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
uniform mat4 u_transform;
out vec3 v_color;
void main() {
  gl_Position = u_transform * vec4(in_position, 1.0);
  v_color = in_color;
}
)";

const char* LINE_FRAGMENT = R"(
#version 330 core
in vec3 v_color;
uniform float u_alpha;
out vec4 fragment;
void main() {
  fragment = vec4(v_color, u_alpha);
}
)";

QString formatTesla(const float& value) {

  const float magnitude = std::fabs(value);

  if (magnitude >= 1.0f) {
    return QString::number((double) value, 'f', 2) + " T";
  }
  if (magnitude >= 1e-3f) {
    return QString::number((double) value * 1e3, 'f', 2) + " mT";
  }
  if (magnitude >= 1e-6f) {
    return QString::number((double) value * 1e6, 'f', 2) + " " + QChar(0x00B5) + "T";
  }

  return QString::number((double) value, 'e', 1) + " T";
}

}  // namespace


viewer::Viewport::Viewport(QWidget* parent): QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(false);
}


viewer::Viewport::~Viewport() {

  // Every buffer belongs to the context, so it has to be let go while the
  // context is still current.
  makeCurrent();

  for (Drawable* drawable : {&magnets_drawable, &slice_drawable,
                             &outlines_drawable, &magnetization_drawable,
                             &glyphs_drawable, &streamlines_drawable,
                             &forces_drawable, &box_drawable, &axes_drawable}) {
    if (drawable->buffer.isCreated()) {
      drawable->buffer.destroy();
    }
    if (drawable->array.isCreated()) {
      drawable->array.destroy();
    }
  }

  surface_program.reset();
  line_program.reset();

  doneCurrent();
}


void viewer::Viewport::setSnapshot(const greeter::view::Snapshot& given) {

  snapshot = given;

  geometry_dirty = true;

  update();
}


void viewer::Viewport::setSettings(const viewer::ViewSettings& given) {

  settings = given;

  geometry_dirty = true;

  update();
}


void viewer::Viewport::frameScene() {

  float low[3];
  float high[3];

  bool framed = snapshot.scene.getBounds(low, high);

  if (snapshot.field.grid && !snapshot.field.empty()) {

    // The field box is usually the thing worth looking at, and often reaches
    // further than the magnets do.
    for (int axis = 0; axis < 3; axis++) {

      const float box_low = snapshot.field.bounds[2 * axis];
      const float box_high = snapshot.field.bounds[2 * axis + 1];

      low[axis] = framed ? std::min(low[axis], box_low) : box_low;
      high[axis] = framed ? std::max(high[axis], box_high) : box_high;
    }

    framed = true;
  }

  if (!framed) {
    target = QVector3D(0.0f, 0.0f, 0.0f);
    distance = 1.0f;
    scene_scale = 1.0f;
    update();
    return;
  }

  target = QVector3D(0.5f * (low[0] + high[0]),
                     0.5f * (low[1] + high[1]),
                     0.5f * (low[2] + high[2]));

  float radius = 0.0f;

  for (int axis = 0; axis < 3; axis++) {
    radius = std::max(radius, 0.5f * (high[axis] - low[axis]));
  }

  // Scenes here are centimetres across, so a camera left at a default of one
  // metre would show an empty room.
  if (!(radius > 0.0f)) {
    radius = 0.05f;
  }

  scene_scale = radius;

  // Far enough back that a sphere of that radius fits the smaller half angle.
  distance = radius / std::tan(0.5f * 45.0f * (float) M_PI / 180.0f) * 1.8f;

  update();
}


void viewer::Viewport::initializeGL() {

  initializeOpenGLFunctions();

  glClearColor(0.11f, 0.12f, 0.14f, 1.0f);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);

  surface_program = std::make_unique<QOpenGLShaderProgram>();
  surface_program->addShaderFromSourceCode(QOpenGLShader::Vertex, SURFACE_VERTEX);
  surface_program->addShaderFromSourceCode(QOpenGLShader::Fragment, SURFACE_FRAGMENT);
  surface_program->link();

  line_program = std::make_unique<QOpenGLShaderProgram>();
  line_program->addShaderFromSourceCode(QOpenGLShader::Vertex, LINE_VERTEX);
  line_program->addShaderFromSourceCode(QOpenGLShader::Fragment, LINE_FRAGMENT);
  line_program->link();

  ready = true;
}


void viewer::Viewport::resizeGL(int width, int height) {
  glViewport(0, 0, width, height);
}


void viewer::Viewport::rebuildGeometry() {

  range = viewer::computeRange(snapshot.field, settings);

  viewer::buildMagnets(snapshot.scene, settings, magnet_surfaces,
                       magnet_outlines, magnetization_arrows, picks);

  viewer::buildSlice(snapshot.field, settings, range, slice_surfaces);
  viewer::buildGlyphs(snapshot.field, settings, range, glyph_lines);
  viewer::buildStreamlines(snapshot.field, settings, streamline_lines);
  viewer::buildForces(snapshot.scene, snapshot.forces, settings, force_lines);

  if (settings.show_field_box) {
    viewer::buildFieldBox(snapshot.field, box_lines);
  } else {
    box_lines.clear();
  }

  if (settings.show_axes) {
    viewer::buildAxes(0.4f * scene_scale, axis_lines);
  } else {
    axis_lines.clear();
  }
}


void viewer::Viewport::upload(Drawable& drawable, const std::vector<float>& data,
                              const int& floats_per_vertex,
                              const bool& with_normal) {

  if (!drawable.array.isCreated()) {
    drawable.array.create();
  }

  if (!drawable.buffer.isCreated()) {
    drawable.buffer.create();
  }

  drawable.vertices = (int) (data.size() / (size_t) floats_per_vertex);

  drawable.array.bind();
  drawable.buffer.bind();

  drawable.buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
  drawable.buffer.allocate(data.data(), (int) (data.size() * sizeof(float)));

  const int stride = floats_per_vertex * (int) sizeof(float);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);

  if (with_normal) {

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));

  } else {

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
  }

  drawable.buffer.release();
  drawable.array.release();
}


void viewer::Viewport::uploadGeometry() {

  upload(magnets_drawable, magnet_surfaces.data, 9, true);
  upload(slice_drawable, slice_surfaces.data, 9, true);

  upload(outlines_drawable, magnet_outlines.data, 6, false);
  upload(magnetization_drawable, magnetization_arrows.data, 6, false);
  upload(glyphs_drawable, glyph_lines.data, 6, false);
  upload(streamlines_drawable, streamline_lines.data, 6, false);
  upload(forces_drawable, force_lines.data, 6, false);
  upload(box_drawable, box_lines.data, 6, false);
  upload(axes_drawable, axis_lines.data, 6, false);
}


QVector3D viewer::Viewport::getEye() const {

  const float horizontal = std::cos(elevation);

  return target + distance * QVector3D(horizontal * std::cos(azimuth),
                                       horizontal * std::sin(azimuth),
                                       std::sin(elevation));
}


QMatrix4x4 viewer::Viewport::getViewMatrix() const {

  QMatrix4x4 view;

  // z is up, as it is everywhere else in this library.
  view.lookAt(getEye(), target, QVector3D(0.0f, 0.0f, 1.0f));

  return view;
}


QMatrix4x4 viewer::Viewport::getProjectionMatrix() const {

  QMatrix4x4 projection;

  const float aspect = height() > 0 ? (float) width() / (float) height() : 1.0f;

  // Tied to the size of the scene, or a centimetre wide scene would be
  // entirely inside the near plane.
  const float near_plane = std::max(1e-6f, 0.01f * scene_scale);
  const float far_plane = std::max(near_plane * 10.0f, 100.0f * scene_scale);

  projection.perspective(45.0f, aspect, near_plane, far_plane);

  return projection;
}


void viewer::Viewport::drawSurfaces(Drawable& drawable, const bool& lit,
                                    const float& alpha) {

  if (drawable.vertices == 0) {
    return;
  }

  surface_program->setUniformValue("u_lit", lit ? 1.0f : 0.0f);
  surface_program->setUniformValue("u_alpha", alpha);

  drawable.array.bind();
  glDrawArrays(GL_TRIANGLES, 0, drawable.vertices);
  drawable.array.release();
}


void viewer::Viewport::drawLines(Drawable& drawable, const float& width) {

  if (drawable.vertices == 0) {
    return;
  }

  glLineWidth(width);

  drawable.array.bind();
  glDrawArrays(GL_LINES, 0, drawable.vertices);
  drawable.array.release();
}


void viewer::Viewport::paintGL() {

  if (!ready) {
    return;
  }

  if (geometry_dirty) {
    rebuildGeometry();
    uploadGeometry();
    geometry_dirty = false;
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);

  const QMatrix4x4 transform = getProjectionMatrix() * getViewMatrix();

  const QVector3D light = (getEye() - target).normalized() +
                          QVector3D(0.3f, 0.2f, 0.6f);

  surface_program->bind();
  surface_program->setUniformValue("u_transform", transform);
  surface_program->setUniformValue("u_light", light);

  // The slice first and without lighting: it stands for a number, and shading
  // it would make the same number look different across the plane.
  //
  // Pushed back a little in depth, because the field lines are started on
  // this very plane and, wherever the field is tangent to it, stay on it. Two
  // things at the same depth is a coin toss, and the lines would come and go
  // as the camera moved. This is the case that matters most: a plane of
  // symmetry is exactly where somebody puts a slice.
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);

  drawSurfaces(slice_drawable, false, 1.0f);

  glDisable(GL_POLYGON_OFFSET_FILL);

  const bool see_through = settings.magnet_opacity < 0.999f;

  if (see_through) {
    // Let the field behind a magnet show through, without the magnet's own
    // far side being drawn over its near side.
    glDepthMask(GL_FALSE);
  }

  drawSurfaces(magnets_drawable, true, settings.magnet_opacity);

  if (see_through) {
    glDepthMask(GL_TRUE);
  }

  surface_program->release();

  line_program->bind();
  line_program->setUniformValue("u_transform", transform);
  line_program->setUniformValue("u_alpha", 1.0f);

  drawLines(box_drawable, 1.0f);
  drawLines(streamlines_drawable, 1.2f);
  drawLines(glyphs_drawable, 1.2f);
  drawLines(outlines_drawable, 1.4f);
  drawLines(magnetization_drawable, 2.0f);
  drawLines(forces_drawable, 2.5f);
  drawLines(axes_drawable, 2.0f);

  line_program->release();

  // The legend and the hint sit on the glass, not in the scene.
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  drawLegend(painter);
  drawHint(painter);

  painter.end();
}


void viewer::Viewport::drawLegend(QPainter& painter) {

  if (snapshot.field.empty()) {
    return;
  }

  const int bar_width = 18;
  const int bar_height = std::min(240, height() - 120);

  if (bar_height < 40) {
    return;
  }

  const int left = width() - 90;
  const int top = 60;

  for (int y = 0; y < bar_height; y++) {

    // Top of the bar is the top of the range.
    const float t = 1.0f - (float) y / (float) (bar_height - 1);

    float rgb[3];

    if (range.diverging) {
      viewer::mapDivergingColor(t, rgb);
    } else {
      viewer::mapColor(t, rgb);
    }

    painter.setPen(QColor::fromRgbF((qreal) rgb[0], (qreal) rgb[1], (qreal) rgb[2]));
    painter.drawLine(left, top + y, left + bar_width, top + y);
  }

  painter.setPen(QColor(210, 212, 218));
  painter.drawRect(left, top, bar_width, bar_height);

  QString title;

  switch (settings.quantity) {
    case viewer::FieldQuantity::Bx: title = "Bx"; break;
    case viewer::FieldQuantity::By: title = "By"; break;
    case viewer::FieldQuantity::Bz: title = "Bz"; break;
    case viewer::FieldQuantity::Magnitude: title = "|B|"; break;
  }

  if (settings.scale == viewer::ColorScale::Logarithmic) {
    title += " (log)";
  }

  painter.drawText(left - 6, top - 12, title);

  painter.drawText(left + bar_width + 4, top + 5, formatTesla(range.maximum));
  painter.drawText(left + bar_width + 4, top + bar_height,
                   formatTesla(range.diverging ? range.minimum : range.minimum));

  if (range.diverging) {
    painter.drawText(left + bar_width + 4, top + bar_height / 2 + 5, "0");
  }
}


void viewer::Viewport::drawHint(QPainter& painter) {

  painter.setPen(QColor(150, 154, 164));

  if (snapshot.scene.empty()) {
    painter.drawText(20, 30, "Open an input file or a snapshot to see something");
    return;
  }

  painter.drawText(20, height() - 18,
                   "drag to turn, middle drag or shift drag to slide, "
                   "wheel to zoom, click a magnet to select it");
}


void viewer::Viewport::getRay(const QPoint& at, QVector3D& origin,
                              QVector3D& direction) const {

  const float x = 2.0f * (float) at.x() / (float) std::max(1, width()) - 1.0f;
  const float y = 1.0f - 2.0f * (float) at.y() / (float) std::max(1, height());

  const QMatrix4x4 transform = getProjectionMatrix() * getViewMatrix();

  bool invertible = false;
  const QMatrix4x4 inverse = transform.inverted(&invertible);

  if (!invertible) {
    origin = getEye();
    direction = (target - origin).normalized();
    return;
  }

  const QVector4D near_point = inverse * QVector4D(x, y, -1.0f, 1.0f);
  const QVector4D far_point = inverse * QVector4D(x, y, 1.0f, 1.0f);

  if (qFuzzyIsNull(near_point.w()) || qFuzzyIsNull(far_point.w())) {
    origin = getEye();
    direction = (target - origin).normalized();
    return;
  }

  origin = near_point.toVector3DAffine();

  direction = (far_point.toVector3DAffine() - origin).normalized();
}


void viewer::Viewport::mousePressEvent(QMouseEvent* event) {

  last_mouse = event->pos();
  press_at = event->pos();
  pressed = event->button();
}


void viewer::Viewport::mouseMoveEvent(QMouseEvent* event) {

  const QPoint moved = event->pos() - last_mouse;

  last_mouse = event->pos();

  if (pressed == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {

    azimuth -= 0.01f * (float) moved.x();
    elevation += 0.01f * (float) moved.y();

    // Stopped just short of the poles, where the up direction flips and the
    // scene appears to spin on its own.
    const float limit = 0.5f * (float) M_PI - 0.01f;

    elevation = std::max(-limit, std::min(limit, elevation));

    update();
    return;
  }

  if (pressed == Qt::MiddleButton ||
      (pressed == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))) {

    const QVector3D forward = (target - getEye()).normalized();
    const QVector3D up(0.0f, 0.0f, 1.0f);

    QVector3D right = QVector3D::crossProduct(forward, up);

    if (right.lengthSquared() <= 0.0f) {
      return;
    }

    right.normalize();

    const QVector3D screen_up = QVector3D::crossProduct(right, forward);

    // A drag slides the scene by as much as it looks like it should, whatever
    // the camera is presently zoomed to.
    const float pace = 2.0f * distance / (float) std::max(1, height());

    target -= right * pace * (float) moved.x();
    target += screen_up * pace * (float) moved.y();

    update();
  }
}


void viewer::Viewport::mouseReleaseEvent(QMouseEvent* event) {

  const bool dragged = (event->pos() - press_at).manhattanLength() > 4;

  if (pressed == Qt::LeftButton && !dragged) {

    QVector3D origin;
    QVector3D direction;

    getRay(event->pos(), origin, direction);

    const float from[3] = {origin.x(), origin.y(), origin.z()};
    const float along[3] = {direction.x(), direction.y(), direction.z()};

    emit magnetPicked((qint64) viewer::pick(picks, from, along));
  }

  pressed = Qt::NoButton;
}


void viewer::Viewport::wheelEvent(QWheelEvent* event) {

  const float notches = (float) event->angleDelta().y() / 120.0f;

  distance *= std::pow(0.85f, notches);

  // Neither inside the scene nor so far out that it is a dot.
  distance = std::max(1e-4f * scene_scale, std::min(1000.0f * scene_scale, distance));

  update();
}
