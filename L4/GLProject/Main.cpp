#include "libs/glad_loader_docs.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include "PolygonWithTexture.cpp"
#include <learnopengl/shader_m.h>
#include "stb_image.h"
#include <learnopengl/camera.h>

using namespace glm;

int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

// 1 = كاميرا من فوق الطاولة، 2 = كاميرا من منظور اللاعب
int currentCamera = 1;

// حالة زر G: نكشف لحظة الضغط فقط (مرة وحدة) وليس كل frame
bool gKeyWasDown = false;
bool gTriggered = false;

// أبعاد آخر صورة تحمّلت (منستخدمها لحساب نسبة صورة ورق الجدران)
int lastTexW = 1;
int lastTexH = 1;

// كاميرا 3 (حرة): نفس كلاس Camera ومتغيرات الماوس من المحاضرة
Camera camera(vec3(0.0f, 1.0f, 3.0f));
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float deltaTime = 0.0f;

void processInput(GLFWwindow *window);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);

// ---------- تحميل Texture ----------
unsigned int loadTexture(const char *path)
{
  unsigned int tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  int w, h, n;
  unsigned char *data = stbi_load(path, &w, &h, &n, 0);
  if (data)
  {
    lastTexW = w;
    lastTexH = h;
    GLenum format = (n == 4) ? GL_RGBA : (n == 1) ? GL_RED : GL_RGB;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  else
    std::cout << "Failed to load texture: " << path << std::endl;
  stbi_image_free(data);
  return tex;
}

// بتجرب .jpg وبعدين .png
unsigned int loadTextureAuto(const std::string &baseName)
{
  std::string jpg = baseName + ".jpg";
  std::string png = baseName + ".png";
  if (std::ifstream(jpg).good())
    return loadTexture(jpg.c_str());
  return loadTexture(png.c_str());
}

// بتدور على baseName.jpg أو baseName.png، وإذا ما لقيت بتستخدم الصورة البديلة
unsigned int loadTextureOr(const std::string &baseName, const char *fallbackPath)
{
  std::string jpg = baseName + ".jpg";
  std::string png = baseName + ".png";
  if (std::ifstream(jpg).good())
    return loadTexture(jpg.c_str());
  if (std::ifstream(png).good())
    return loadTexture(png.c_str());
  return loadTexture(fallbackPath);
}

// ---------- دوال مساعدة لبناء الأشكال ----------

// مستطيل من 4 نقاط
PolygonWithTexture makeQuad(vec3 a, vec3 b, vec3 c, vec3 d, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<vec3> v = {a, b, c, d};
  std::vector<vec2> t = {vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
  PolygonWithTexture q(v, t, color);
  q.setTexture(tex);
  return q;
}

// مستطيل مع تكرار الـ Texture (للأرضية)
PolygonWithTexture makeTiledQuad(vec3 a, vec3 b, vec3 c, vec3 d, GLuint &tex, float repU, float repV)
{
  std::vector<vec3> v = {a, b, c, d};
  std::vector<vec2> t = {vec2(0, 0), vec2(repU, 0), vec2(repU, repV), vec2(0, repV)};
  PolygonWithTexture q(v, t);
  q.setTexture(tex);
  return q;
}

// جدار بدون خطوط: صورة وحدة بالارتفاع (مقصوصة من الأعلى والأسفل لتخبئة حافة الصورة)
// وبتتكرر أفقياً فقط، وبنسبة الصورة الأصلية حتى ما تتشوه.
const float WALL_V_INSET = 0.04f; // كم نقص من أطراف الصورة عمودياً
PolygonWithTexture makeWall(vec3 a, vec3 b, vec3 c, vec3 d, float wallW, float wallH, float aspect, GLuint &tex)
{
  float tileH = wallH / (1.0f - 2.0f * WALL_V_INSET); // ارتفاع الصورة الكاملة بوحدات المشهد
  float tileW = tileH * aspect;
  float repU = wallW / tileW;
  std::vector<vec3> v = {a, b, c, d};
  std::vector<vec2> t = {vec2(0, WALL_V_INSET), vec2(repU, WALL_V_INSET),
                         vec2(repU, 1.0f - WALL_V_INSET), vec2(0, 1.0f - WALL_V_INSET)};
  PolygonWithTexture q(v, t);
  q.setTexture(tex);
  return q;
}

// مستطيل أفقي مستلقي (على ارتفاع y)
PolygonWithTexture makeFlatRect(float cx, float cz, float w, float d, float y, GLuint &tex, vec3 color = vec3(1.0f))
{
  return makeQuad(vec3(cx - w / 2, y, cz + d / 2),
                  vec3(cx + w / 2, y, cz + d / 2),
                  vec3(cx + w / 2, y, cz - d / 2),
                  vec3(cx - w / 2, y, cz - d / 2),
                  tex, color);
}

// صندوق (6 وجوه). كل وجه بيتغمّق حسب اتجاهه (إضاءة وهمية): فوق أفتح، تحت أغمق
std::vector<PolygonWithTexture> makeBox(float cx, float cy, float cz, float w, float h, float d, GLuint &tex, vec3 color = vec3(1.0f))
{
  float x0 = cx - w / 2, x1 = cx + w / 2;
  float y0 = cy - h / 2, y1 = cy + h / 2;
  float z0 = cz - d / 2, z1 = cz + d / 2;
  const float SH_TOP = 1.00f, SH_BOTTOM = 0.45f, SH_FRONT = 0.86f, SH_BACK = 0.60f, SH_LEFT = 0.78f, SH_RIGHT = 0.66f;
  std::vector<PolygonWithTexture> faces;
  faces.push_back(makeQuad(vec3(x0, y1, z1), vec3(x1, y1, z1), vec3(x1, y1, z0), vec3(x0, y1, z0), tex, color * SH_TOP));       // أعلى
  faces.push_back(makeQuad(vec3(x0, y0, z0), vec3(x1, y0, z0), vec3(x1, y0, z1), vec3(x0, y0, z1), tex, color * SH_BOTTOM));    // أسفل
  faces.push_back(makeQuad(vec3(x0, y0, z1), vec3(x1, y0, z1), vec3(x1, y1, z1), vec3(x0, y1, z1), tex, color * SH_FRONT));     // أمام
  faces.push_back(makeQuad(vec3(x1, y0, z0), vec3(x0, y0, z0), vec3(x0, y1, z0), vec3(x1, y1, z0), tex, color * SH_BACK));      // خلف
  faces.push_back(makeQuad(vec3(x0, y0, z0), vec3(x0, y0, z1), vec3(x0, y1, z1), vec3(x0, y1, z0), tex, color * SH_LEFT));      // يسار
  faces.push_back(makeQuad(vec3(x1, y0, z1), vec3(x1, y0, z0), vec3(x1, y1, z0), vec3(x1, y1, z1), tex, color * SH_RIGHT));     // يمين
  return faces;
}

// صندوق مائل حول محور X (للمساند والوسادات)
std::vector<PolygonWithTexture> makeTiltedBoxX(float cx, float cy, float cz, float w, float h, float d, float angleDeg, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<PolygonWithTexture> faces = makeBox(0, 0, 0, w, h, d, tex, color);
  mat4 m = translate(mat4(1.0f), vec3(cx, cy, cz));
  m = rotate(m, radians(angleDeg), vec3(1.0f, 0.0f, 0.0f));
  for (auto &f : faces)
    f.transformation(m);
  return faces;
}

// عمود رفيع بين نقطتين بمستوى YZ (لرسم قوس المصباح من قطع قصيرة)
std::vector<PolygonWithTexture> makeRodYZ(float x, float y0, float z0, float y1, float z1, float thick, GLuint &tex, vec3 color)
{
  float dy = y1 - y0, dz = z1 - z0;
  float len = std::sqrt(dy * dy + dz * dz);
  float angle = degrees(std::atan2(dz, dy));
  return makeTiltedBoxX(x, (y0 + y1) / 2, (z0 + z1) / 2, thick, len * 1.05f, thick, angle, tex, color);
}

// أسطوانة / مخروط ناقص. كل قطعة من الجدار بتاخد درجة إضاءة حسب اتجاهها فبتبين مدوّرة
std::vector<PolygonWithTexture> makeCylinder(float cx, float cy, float cz, float rBottom, float rTop, float h, int segments, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<PolygonWithTexture> faces;
  float y0 = cy - h / 2.0f;
  float y1 = cy + h / 2.0f;
  float lx = -0.5f, lz = 0.85f; // اتجاه الضوء (من اليسار والأمام)
  float lLen = std::sqrt(lx * lx + lz * lz);
  for (int i = 0; i < segments; i++)
  {
    float a0 = (2.0f * 3.1415926f * i) / segments;
    float a1 = (2.0f * 3.1415926f * (i + 1)) / segments;
    float am = (a0 + a1) / 2.0f;
    float nd = (std::cos(am) * lx + std::sin(am) * lz) / lLen;
    float shade = 0.55f + 0.45f * (nd > 0.0f ? nd : 0.0f);

    vec3 b0(cx + rBottom * std::cos(a0), y0, cz + rBottom * std::sin(a0));
    vec3 b1(cx + rBottom * std::cos(a1), y0, cz + rBottom * std::sin(a1));
    vec3 t1(cx + rTop * std::cos(a1), y1, cz + rTop * std::sin(a1));
    vec3 t0(cx + rTop * std::cos(a0), y1, cz + rTop * std::sin(a0));
    faces.push_back(makeQuad(b0, b1, t1, t0, tex, color * shade));                              // الجدار
    faces.push_back(makeQuad(vec3(cx, y1, cz), t0, t1, vec3(cx, y1, cz), tex, color));          // الغطاء العلوي
    faces.push_back(makeQuad(vec3(cx, y0, cz), b1, b0, vec3(cx, y0, cz), tex, color * 0.45f));  // القاعدة السفلية
  }
  return faces;
}

// ---------- أشكال بزوايا مدوّرة (للطاولة وسطحها) ----------

// نقاط مستطيل بزوايا مدوّرة بمستوى XZ (متمركز بالأصل)، بالترتيب حوالين المحيط
std::vector<vec2> roundedOutline(float w, float d, float r, int seg)
{
  std::vector<vec2> pts;
  vec2 centers[4] = {vec2(w / 2 - r, d / 2 - r), vec2(-w / 2 + r, d / 2 - r),
                     vec2(-w / 2 + r, -d / 2 + r), vec2(w / 2 - r, -d / 2 + r)};
  for (int c = 0; c < 4; c++)
    for (int i = 0; i <= seg; i++)
    {
      float a = radians(90.0f * c + 90.0f * i / seg);
      pts.push_back(vec2(centers[c].x + r * std::cos(a), centers[c].y + r * std::sin(a)));
    }
  return pts;
}

// مضلع أفقي مدوّر الزوايا (مضلع محدب، بيترسم بـ GL_TRIANGLE_FAN)
PolygonWithTexture makeRoundedFlat(float cx, float cz, float w, float d, float y, float r, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<vec2> o = roundedOutline(w, d, r, 8);
  std::vector<vec3> v;
  std::vector<vec2> t;
  for (auto &p : o)
  {
    v.push_back(vec3(cx + p.x, y, cz + p.y));
    t.push_back(vec2(p.x / w + 0.5f, p.y / d + 0.5f));
  }
  PolygonWithTexture q(v, t, color);
  q.setTexture(tex);
  return q;
}

// لوح سميك بزوايا مدوّرة: وجه علوي + سفلي + جوانب (قطعة لكل ضلع من محيط الشكل)
std::vector<PolygonWithTexture> makeRoundedSlab(float cx, float cy, float cz, float w, float d, float h, float r, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<PolygonWithTexture> faces;
  float y1 = cy + h / 2, y0 = cy - h / 2;
  faces.push_back(makeRoundedFlat(cx, cz, w, d, y1, r, tex, color));
  faces.push_back(makeRoundedFlat(cx, cz, w, d, y0, r, tex, color * 0.45f));

  std::vector<vec2> o = roundedOutline(w, d, r, 8);
  float lx = -0.5f, lz = 0.85f;
  float lLen = std::sqrt(lx * lx + lz * lz);
  float run = 0.0f; // الطول التراكمي لتوزيع الـ Texture على الجوانب
  for (size_t i = 0; i < o.size(); i++)
  {
    vec2 a = o[i], b = o[(i + 1) % o.size()];
    float dx = b.x - a.x, dz = b.y - a.y;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len < 0.00001f)
      continue;
    float nd = (dz * lx - dx * lz) / (len * lLen); // الضوء على الضلع حسب اتجاهه
    float shade = 0.55f + 0.45f * (nd > 0.0f ? nd : 0.0f);
    float u0 = run * 0.4f, u1 = (run + len) * 0.4f;
    std::vector<vec3> v = {vec3(cx + a.x, y0, cz + a.y), vec3(cx + b.x, y0, cz + b.y),
                           vec3(cx + b.x, y1, cz + b.y), vec3(cx + a.x, y1, cz + a.y)};
    std::vector<vec2> t = {vec2(u0, 0), vec2(u1, 0), vec2(u1, 1), vec2(u0, 1)};
    PolygonWithTexture q(v, t, color * shade);
    q.setTexture(tex);
    faces.push_back(q);
    run += len;
  }
  return faces;
}

// ---------- بوستر ممزق/مقطوع من الزاوية العلوية اليمنى ----------
PolygonWithTexture makeTornPoster(float x0, float y0, float w, float h, float z, GLuint &tex)
{
  float x1 = x0 + w;
  float y1 = y0 + h;

  std::vector<vec3> v = {
      vec3(x0, y0, z),
      vec3(x1, y0, z),
      vec3(x1, y0 + h * 0.55f, z),
      vec3(x0 + w * 0.82f, y0 + h * 0.68f, z),
      vec3(x0 + w * 0.72f, y0 + h * 0.80f, z),
      vec3(x0 + w * 0.55f, y1, z),
      vec3(x0, y1, z)};

  std::vector<vec2> t = {
      vec2(0.0f, 0.0f),
      vec2(1.0f, 0.0f),
      vec2(1.0f, 0.55f),
      vec2(0.82f, 0.68f),
      vec2(0.72f, 0.80f),
      vec2(0.55f, 1.0f),
      vec2(0.0f, 1.0f)};

  PolygonWithTexture poster(v, t);
  poster.setTexture(tex);
  return poster;
}

// ---------- البطاقة (مودرن: أصغر + زوايا مدوّرة + إطار رفيع) ----------
const float CARD_W = 0.40f;
const float CARD_H = 0.64f;
const float CARD_RADIUS = 0.05f;   // استدارة الزوايا
const float CARD_UV_INSET = 0.04f; // قص من أطراف الصورة لإخفاء الحافة السوداء
const float CARD_GAP = 0.60f;      // المسافة بين مراكز البطاقات
const float HAND_Z = 1.32f;        // الحافة الأمامية للبطاقات بالمخزون
const float PLAYED_Z = 0.32f;      // وبمنطقة اللعب
const float STAND_TILT = -10.0f;   // ميل البطاقة الواقفة للخلف

// ثوابت الأنيميشن
const float MOVE_DURATION = 0.6f;  // مدة انتقال البطاقة بالثواني
const float LIFT = 0.35f;          // ارتفاع القوس أثناء الطيران
const float STAND_DURATION = 0.5f; // مدة الوقوف/التسطيح

float cardSlotX(int i) { return (i - 2) * CARD_GAP; } // 5 أماكن متناظرة حول النص

// مستطيل بزوايا مدوّرة (مضلع محدب بيترسم بـ GL_TRIANGLE_FAN)
PolygonWithTexture makeRoundedCard(float w, float h, float y0, float r, float uvInset, GLuint &tex, vec3 color = vec3(1.0f))
{
  std::vector<vec3> v;
  std::vector<vec2> t;
  const int SEG = 6; // عدد القطع بكل زاوية
  vec2 centers[4] = {vec2(w / 2 - r, y0 + r), vec2(w / 2 - r, y0 + h - r),
                     vec2(-w / 2 + r, y0 + h - r), vec2(-w / 2 + r, y0 + r)};
  float startAngle[4] = {-90.0f, 0.0f, 90.0f, 180.0f};
  for (int c = 0; c < 4; c++)
    for (int i = 0; i <= SEG; i++)
    {
      float a = radians(startAngle[c] + 90.0f * i / SEG);
      float x = centers[c].x + r * std::cos(a);
      float y = centers[c].y + r * std::sin(a);
      v.push_back(vec3(x, y, 0.0f));
      // إحداثيات الـ Texture من الموقع، مع قص uvInset من كل طرف
      float u = uvInset + (x + w / 2) / w * (1.0f - 2 * uvInset);
      float vv = uvInset + (y - y0) / h * (1.0f - 2 * uvInset);
      t.push_back(vec2(u, vv));
    }
  PolygonWithTexture p(v, t, color);
  p.setTexture(tex);
  return p;
}

struct Card
{
  PolygonWithTexture face;  // وجه البطاقة (صورة الشخصية)
  PolygonWithTexture frame; // إطار غامق رفيع خلف الوجه
  vec3 position;
  vec3 startPos, targetPos;
  float progress = 1.0f; // 1 = واصلة، أقل = عم تتحرك

  Card(GLuint &tex, GLuint &whiteTex, vec3 pos)
      : face(makeRoundedCard(CARD_W, CARD_H, 0.0f, CARD_RADIUS, CARD_UV_INSET, tex)),
        frame(makeRoundedCard(CARD_W + 0.03f, CARD_H + 0.03f, -0.015f, CARD_RADIUS + 0.015f, 0.0f, whiteTex, vec3(0.10f, 0.10f, 0.12f))),
        position(pos) {}

  void moveTo(vec3 target)
  {
    startPos = position;
    targetPos = target;
    progress = 0.0f;
  }

  void update(float dt)
  {
    if (progress >= 1.0f)
      return;
    progress = std::min(progress + dt / MOVE_DURATION, 1.0f);
    float s = progress * progress * (3.0f - 2.0f * progress); // smoothstep
    position = glm::mix(startPos, targetPos, s);
    position.y += std::sin(progress * 3.1415926f) * LIFT; // قوس
    if (progress >= 1.0f)
      position = targetPos;
  }
  // stand: 0 = مسطحة، 1 = واقفة
  void draw(Shader &shader, float stand)
  {
    float angle = glm::mix(-90.0f, STAND_TILT, stand);
    mat4 model = translate(mat4(1.0f), position);
    model = rotate(model, radians(angle), vec3(1.0f, 0.0f, 0.0f));
    frame.transformation(translate(model, vec3(0.0f, 0.0f, -0.004f))); // الإطار وراء الوجه بشوي
    frame.draw(shader);
    face.transformation(model);
    face.draw(shader);
  }
};

// ---------- أدوات إضافية للديكور ----------
// قرص دائري بالمستوى XY (المركز أول نقطة بالـ fan)
PolygonWithTexture makeDisc(float r, int seg, GLuint &tex, vec3 color)
{
  std::vector<vec3> v;
  std::vector<vec2> t;
  v.push_back(vec3(0.0f));
  t.push_back(vec2(0.5f));
  for (int i = 0; i <= seg; i++)
  {
    float a = 2.0f * 3.1415926f * i / seg;
    v.push_back(vec3(r * std::cos(a), r * std::sin(a), 0.0f));
    t.push_back(vec2(0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)));
  }
  PolygonWithTexture d(v, t, color);
  d.setTexture(tex);
  return d;
}

// عقرب: مستطيل رفيع بيبدأ من المركز ويطلع لفوق
PolygonWithTexture makeHand(float len, float thick, GLuint &tex, vec3 color)
{
  return makeQuad(vec3(-thick / 2, -len * 0.12f, 0), vec3(thick / 2, -len * 0.12f, 0),
                  vec3(thick / 2, len, 0), vec3(-thick / 2, len, 0), tex, color);
}

// مثلث قاعدته على y=0 ورأسه لفوق (للهب)
PolygonWithTexture makeTriangle(float w, float h, GLuint &tex, vec3 color)
{
  std::vector<vec3> v = {vec3(-w / 2, 0, 0), vec3(w / 2, 0, 0), vec3(0, h, 0)};
  std::vector<vec2> t = {vec2(0, 0), vec2(1, 0), vec2(0.5f, 1)};
  PolygonWithTexture tri(v, t, color);
  tri.setTexture(tex);
  return tri;
}

// لهب متحرك: المكان الأساسي + مرحلة وسرعة الاهتزاز
struct Flame
{
  PolygonWithTexture poly;
  vec3 base;
  float phase;
  float speed;
};

// ورقة نبتة: ساق رفيعة + نصل بيضاوي (مضلع محدب بـ fan). كلها بمستوى XY وبتتحول بمصفوفة base
std::vector<PolygonWithTexture> makeLeaf(float stemLen, float bladeLen, float bladeW, GLuint &tex,
                                         vec3 stemCol, vec3 bladeCol, mat4 base, float droop)
{
  std::vector<PolygonWithTexture> parts;
  PolygonWithTexture stem = makeQuad(vec3(-0.012f, 0, 0), vec3(0.012f, 0, 0),
                                     vec3(0.012f, stemLen, 0), vec3(-0.012f, stemLen, 0), tex, stemCol);
  stem.transformation(base);
  parts.push_back(stem);

  const int N = 8;
  std::vector<vec3> v;
  std::vector<vec2> t;
  v.push_back(vec3(0, 0, 0));
  t.push_back(vec2(0.5f, 0.0f));
  for (int i = 1; i <= N; i++) // الجهة اليمنى من الورقة
  {
    float s = (float)i / (N + 1);
    float w = bladeW * std::pow(std::sin(3.1415926f * s), 0.8f);
    v.push_back(vec3(w, bladeLen * s, 0));
    t.push_back(vec2(0.5f + 0.5f * w / bladeW, s));
  }
  v.push_back(vec3(0, bladeLen, 0)); // رأس الورقة
  t.push_back(vec2(0.5f, 1.0f));
  for (int i = N; i >= 1; i--) // الجهة اليسرى
  {
    float s = (float)i / (N + 1);
    float w = bladeW * std::pow(std::sin(3.1415926f * s), 0.8f);
    v.push_back(vec3(-w, bladeLen * s, 0));
    t.push_back(vec2(0.5f - 0.5f * w / bladeW, s));
  }
  PolygonWithTexture blade(v, t, bladeCol);
  blade.setTexture(tex);
  mat4 bm = translate(base, vec3(0, stemLen, 0));          // النصل بيبدأ من نهاية الساق
  bm = rotate(bm, radians(droop), vec3(1.0f, 0.0f, 0.0f)); // ميل إضافي (تدلّي)
  blade.transformation(bm);
  parts.push_back(blade);
  return parts;
}

// ---------- ستائر وأسماك ----------
// ستارة: شرائح عمودية بتتبادل بالعمق (طيّات). dir = +1 للجدار اليسار، -1 للجدار اليمين
std::vector<PolygonWithTexture> makeCurtainPanel(float wallX, float dir, float zA, float zB, float yBot, float yTop,
                                                 int folds, float depth, GLuint &tex, vec3 color)
{
  std::vector<PolygonWithTexture> parts;
  const float BASE = 0.03f; // أقرب مسافة عن الجدار
  for (int i = 0; i < folds; i++)
  {
    float z0 = zA + (zB - zA) * i / folds;
    float z1 = zA + (zB - zA) * (i + 1) / folds;
    float o0 = (i % 2 == 0) ? BASE : BASE + depth;
    float o1 = (i % 2 == 0) ? BASE + depth : BASE;
    float shade = (i % 2 == 0) ? 0.82f : 1.0f; // إضاءة وهمية بتبين الطيّات
    parts.push_back(makeQuad(vec3(wallX + dir * o0, yBot, z0), vec3(wallX + dir * o1, yBot, z1),
                             vec3(wallX + dir * o1, yTop, z1), vec3(wallX + dir * o0, yTop, z0), tex, color * shade));
  }
  return parts;
}

// سمكة بالحوض: جسم (قرص جانبي) + قرص أفقي رفيع (سماكة البطن) + ذيل (مثلث) + عين
// بتسبح بـ 3 أبعاد جوا الماء على مسار بيضاوي (x,z) مع تموّج بسيط بالارتفاع
struct Fish
{
  PolygonWithTexture body, belly, tail, eye;
  vec2 center, radius; // مركز المسار البيضاوي ونصفي قطره (x,z)
  float speed, phase, dir;
  float yBase; // الارتفاع الأساسي للسمكة جوا الماء
};

Fish makeFish(GLuint &tex, vec3 color, vec2 center, vec2 radius, float speed, float phase, float dir, float yBase)
{
  PolygonWithTexture body = makeDisc(1.0f, 24, tex, color);
  PolygonWithTexture belly = makeDisc(1.0f, 24, tex, color * 0.75f);
  std::vector<vec3> tv = {vec3(0, 0, 0), vec3(-0.14f, 0.08f, 0), vec3(-0.14f, -0.08f, 0)};
  std::vector<vec2> tt = {vec2(0.5f, 0), vec2(0, 1), vec2(1, 1)};
  PolygonWithTexture tail(tv, tt, color * 0.8f);
  tail.setTexture(tex);
  PolygonWithTexture eye = makeDisc(0.018f, 10, tex, vec3(0.05f));
  return {body, belly, tail, eye, center, radius, speed, phase, dir, yBase};
}

int main()
{
  glfwInit();
  GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Card Game", NULL, NULL);
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);

  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  glEnable(GL_DEPTH_TEST);
  stbi_set_flip_vertically_on_load(true);

  Shader ourShader("./shaders/vs/L4.vs", "./shaders/fs/L4.fs");

  // ---------- Textures ----------
  unsigned int whiteTex = loadTexture("./textures/white.jpg");
  unsigned int woodTex = loadTextureOr("./textures/wood2", "./textures/crate.jpg");
  unsigned int posterTex = loadTextureOr("./textures/poster", "./textures/card1.jpg");

  // ---------- الطاولة: سطح مدوّر الزوايا + 4 أرجل ----------
  std::vector<PolygonWithTexture> table = makeRoundedSlab(0.0f, -0.15f, 0.0f, 4.0f, 3.0f, 0.3f, 0.40f, woodTex);
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2)
    {
      std::vector<PolygonWithTexture> leg = makeBox(sx * 1.85f, -0.65f, sz * 1.35f, 0.2f, 0.7f, 0.2f, woodTex);
      table.insert(table.end(), leg.begin(), leg.end());
    }

  // ---------- الغرفة ----------
  unsigned int floorTex = loadTextureOr("./textures/wood1", "./textures/crate.jpg");
  unsigned int wallTex = loadTextureOr("./textures/wallpaper", "./textures/wall.jpg");
  float wallAspect = (float)lastTexW / (float)lastTexH; // نسبة صورة ورق الجدران
  float wallH = 4.2f;                                   // من y=-1 إلى y=3.2

  // أبعاد الغرفة (صغّريها أو كبّريها من هون وكل شي بيتحرك معها)
  const float ROOM_X = 4.4f;       // نص عرض الغرفة (من -ROOM_X إلى +ROOM_X)
  const float ROOM_BACK_Z = -5.4f; // الجدار الخلفي
  const float ROOM_FRONT_Z = 4.2f; // الجدار الأمامي (ورا الكاميرا)
  float dz = ROOM_BACK_Z + 5.0f;   // إزاحة الديكور مع الجدار الخلفي (القيم الأصلية كانت لجدار عند -5)
  float roomW = 2 * ROOM_X;
  float roomD = ROOM_FRONT_Z - ROOM_BACK_Z;

  std::vector<PolygonWithTexture> room;
  // الأرضية (التكرار بيتناسب مع المساحة)
  room.push_back(makeTiledQuad(vec3(-ROOM_X, -1, ROOM_FRONT_Z), vec3(ROOM_X, -1, ROOM_FRONT_Z), vec3(ROOM_X, -1, ROOM_BACK_Z), vec3(-ROOM_X, -1, ROOM_BACK_Z), floorTex, roomW * 0.77f, roomD * 0.77f));
  // الجدار الخلفي
  room.push_back(makeWall(vec3(-ROOM_X, -1, ROOM_BACK_Z), vec3(ROOM_X, -1, ROOM_BACK_Z), vec3(ROOM_X, 3.2f, ROOM_BACK_Z), vec3(-ROOM_X, 3.2f, ROOM_BACK_Z), roomW, wallH, wallAspect, wallTex));
  // الجدار الأمامي
  room.push_back(makeWall(vec3(ROOM_X, -1, ROOM_FRONT_Z), vec3(-ROOM_X, -1, ROOM_FRONT_Z), vec3(-ROOM_X, 3.2f, ROOM_FRONT_Z), vec3(ROOM_X, 3.2f, ROOM_FRONT_Z), roomW, wallH, wallAspect, wallTex));
  // الجدار اليسار
  room.push_back(makeWall(vec3(-ROOM_X, -1, ROOM_FRONT_Z), vec3(-ROOM_X, -1, ROOM_BACK_Z), vec3(-ROOM_X, 3.2f, ROOM_BACK_Z), vec3(-ROOM_X, 3.2f, ROOM_FRONT_Z), roomD, wallH, wallAspect, wallTex));
  // الجدار اليمين
  room.push_back(makeWall(vec3(ROOM_X, -1, ROOM_BACK_Z), vec3(ROOM_X, -1, ROOM_FRONT_Z), vec3(ROOM_X, 3.2f, ROOM_FRONT_Z), vec3(ROOM_X, 3.2f, ROOM_BACK_Z), roomD, wallH, wallAspect, wallTex));

  // ---------- البوستر الممزق ----------
  PolygonWithTexture poster = makeTornPoster(-0.65f, -0.15f, 1.3f, 1.45f, ROOM_BACK_Z + 0.02f, posterTex);

  // ---------- الديكور: كونسول+مراية، مدفأة+ساعة، حوض ----------
  std::vector<PolygonWithTexture> decor;
  auto addBox = [&](float cx, float cy, float cz, float w, float h, float d, GLuint &tex, vec3 color = vec3(1.0f))
  {
    std::vector<PolygonWithTexture> b = makeBox(cx, cy, cz, w, h, d, tex, color);
    decor.insert(decor.end(), b.begin(), b.end());
  };
  auto addPart = [&](const std::vector<PolygonWithTexture> &part)
  {
    decor.insert(decor.end(), part.begin(), part.end());
  };
  auto addQuad = [&](vec3 a, vec3 b, vec3 c, vec3 d, vec3 color)
  {
    decor.push_back(makeQuad(a, b, c, d, whiteTex, color));
  };

  // أداة: قرص دائري بمكان محدد (للمراية والساعة)
  auto addDisc = [&](float x, float y, float z, float r, vec3 color)
  {
    PolygonWithTexture d = makeDisc(r, 48, whiteTex, color);
    d.transformation(translate(mat4(1.0f), vec3(x, y, z)));
    decor.push_back(d);
  };
  (void)addDisc;

  // أداة: شمعدان (قاعدة + ساق + شمعة + شعلة)
  auto addCandlestick = [&](float x, float baseY, float z, float h)
  {
    vec3 brass(0.78f, 0.62f, 0.25f);
    addPart(makeCylinder(x, baseY + 0.015f, z, 0.07f, 0.06f, 0.03f, 16, whiteTex, brass));
    addPart(makeCylinder(x, baseY + 0.03f + h * 0.5f, z, 0.015f, 0.015f, h, 10, whiteTex, brass));
    addPart(makeCylinder(x, baseY + 0.03f + h + 0.08f, z, 0.03f, 0.03f, 0.16f, 12, whiteTex, vec3(0.95f, 0.92f, 0.82f)));
    addPart(makeCylinder(x, baseY + 0.03f + h + 0.19f, z, 0.016f, 0.004f, 0.06f, 8, whiteTex, vec3(1.0f, 0.75f, 0.2f)));
  };

  // أداة: مزهرية (جسم + عنق) مع أغصان أوكالبتوس اختيارية
  auto addVase = [&](float x, float baseY, float z, vec3 col, bool branches)
  {
    addPart(makeCylinder(x, baseY + 0.18f, z, 0.10f, 0.07f, 0.36f, 18, whiteTex, col));
    addPart(makeCylinder(x, baseY + 0.38f, z, 0.045f, 0.055f, 0.06f, 18, whiteTex, col));
    if (branches)
    {
      vec3 leaf(0.12f, 0.20f, 0.14f);
      for (int sx = -1; sx <= 1; sx += 2)
      {
        addPart(makeTiltedBoxX(x + sx * 0.04f, baseY + 0.66f, z, 0.012f, 0.50f, 0.012f, sx * 12.0f, whiteTex, leaf));
        for (int i = 0; i < 5; i++)
          addBox(x + sx * (0.04f + (i % 2 ? 0.05f : -0.01f)), baseY + 0.50f + i * 0.09f, z, 0.09f, 0.05f, 0.012f, whiteTex, leaf);
      }
    }
  };

  // لهب المدفأة (بيتحرك بالـ loop)
  std::vector<Flame> flames;

  // ساعة الحائط (بتتحرك بالـ loop)
  vec3 clockPos(2.8f, 1.65f, ROOM_BACK_Z + 0.03f);
  mat4 clockBase = translate(mat4(1.0f), clockPos);
  PolygonWithTexture clockRim = makeDisc(0.46f, 40, whiteTex, vec3(0.20f, 0.12f, 0.07f));
  PolygonWithTexture clockFace = makeDisc(0.40f, 40, whiteTex, vec3(0.93f, 0.90f, 0.80f));
  PolygonWithTexture hourHand = makeHand(0.22f, 0.035f, whiteTex, vec3(0.08f));
  PolygonWithTexture minuteHand = makeHand(0.32f, 0.025f, whiteTex, vec3(0.08f));
  PolygonWithTexture secondHand = makeHand(0.34f, 0.010f, whiteTex, vec3(0.75f, 0.10f, 0.10f));
  clockRim.transformation(clockBase);
  clockFace.transformation(translate(clockBase, vec3(0, 0, 0.01f)));

  // ===== 1) كونسول أسود + مراية دائرية مضيئة + شمعدانات على الجدار اليسار =====
  {
    float zc = -2.15f;     // مركز الكونسول بإتجاه z
    float wx = -ROOM_X;    // الجدار اليسار
    float cx = wx + 0.25f; // مركز الكونسول (عمقه 0.42 ملاصق للجدار)
    vec3 black(0.08f, 0.08f, 0.09f);
    float topY = -0.16f;
    // الكونسول: سطح + جانبين
    addBox(cx, -0.20f, zc, 0.42f, 0.08f, 1.7f, whiteTex, black);
    addBox(cx, -0.62f, zc - 0.78f, 0.42f, 0.76f, 0.12f, whiteTex, black);
    addBox(cx, -0.62f, zc + 0.78f, 0.42f, 0.76f, 0.12f, whiteTex, black);

    // المراية: أقراص على الجدار اليسار، بنلفها 90 حول Y حتى تواجه +x (جوا الغرفة)
    auto addDiscWall = [&](float y, float off, float r, vec3 color)
    {
      PolygonWithTexture d = makeDisc(r, 48, whiteTex, color);
      mat4 m = translate(mat4(1.0f), vec3(wx + off, y, zc));
      m = rotate(m, radians(90.0f), vec3(0.0f, 1.0f, 0.0f));
      d.transformation(m);
      decor.push_back(d);
    };
    addDiscWall(0.95f, 0.012f, 0.92f, vec3(1.0f, 0.82f, 0.50f));  // الهالة
    addDiscWall(0.95f, 0.022f, 0.83f, black);                     // الإطار
    addDiscWall(0.95f, 0.032f, 0.79f, vec3(0.55f, 0.62f, 0.68f)); // سطح المراية

    // موزع عطر (جرة سوداء + عيدان)
    addPart(makeCylinder(cx, topY + 0.10f, zc - 0.66f, 0.08f, 0.07f, 0.20f, 18, whiteTex, black));
    addPart(makeCylinder(cx, topY + 0.215f, zc - 0.66f, 0.03f, 0.03f, 0.03f, 12, whiteTex, vec3(0.78f, 0.62f, 0.25f)));
    for (int k = -2; k <= 2; k++)
      addPart(makeTiltedBoxX(cx + k * 0.015f, topY + 0.47f, zc - 0.66f, 0.008f, 0.55f, 0.008f, k * 5.0f, whiteTex, black));

    // كتابين + شمعة بجرة سوداء
    addBox(cx, topY + 0.03f, zc - 0.25f, 0.30f, 0.06f, 0.42f, whiteTex, vec3(0.90f, 0.90f, 0.88f));
    addBox(cx + 0.01f, topY + 0.09f, zc - 0.23f, 0.28f, 0.06f, 0.40f, whiteTex, black);
    addPart(makeCylinder(cx, topY + 0.18f, zc - 0.25f, 0.07f, 0.07f, 0.12f, 16, whiteTex, black));
    addPart(makeCylinder(cx, topY + 0.245f, zc - 0.25f, 0.05f, 0.05f, 0.01f, 16, whiteTex, vec3(1.0f, 0.85f, 0.45f)));

    // شمعدانين بطولين مختلفين + مزهرية بأغصان
    addCandlestick(cx, topY, zc + 0.12f, 0.18f);
    addCandlestick(cx, topY, zc + 0.34f, 0.30f);
    addVase(cx, topY, zc + 0.66f, vec3(0.92f, 0.92f, 0.90f), true);
  }

  // ===== 2) مدفأة حطب + ساعة حائط فوقها + شمعدانات ومزهريات =====
  {
    float cx = 2.8f, cz = ROOM_BACK_Z + 0.25f;
    vec3 stone(0.62f, 0.58f, 0.52f);
    float fz = cz + 0.25f + 0.005f; // الوجه الأمامي للمدفأة

    addBox(cx, -0.25f, cz, 1.9f, 1.5f, 0.5f, whiteTex, stone);                              // الجسم
    addBox(cx, 0.54f, cz + 0.08f, 2.2f, 0.08f, 0.65f, woodTex, vec3(0.55f, 0.40f, 0.30f));  // الرف
    addBox(cx, -0.98f, cz + 0.4f, 2.1f, 0.04f, 0.8f, whiteTex, vec3(0.35f, 0.33f, 0.32f));  // الأرضية
    addQuad(vec3(cx - 0.55f, -1.0f, fz), vec3(cx + 0.55f, -1.0f, fz),
            vec3(cx + 0.55f, 0.0f, fz), vec3(cx - 0.55f, 0.0f, fz), vec3(0.04f, 0.03f, 0.03f)); // فتحة النار

    // حطب
    addBox(cx, -0.92f, fz + 0.05f, 0.8f, 0.09f, 0.09f, woodTex, vec3(0.45f, 0.28f, 0.16f));
    addBox(cx + 0.05f, -0.84f, fz + 0.08f, 0.6f, 0.08f, 0.08f, woodTex, vec3(0.40f, 0.24f, 0.14f));

    // لهب: 5 مثلثات برتقالية + 5 أصغر صفراء بنصهم
    float fx[5] = {-0.36f, -0.18f, 0.0f, 0.18f, 0.36f};
    float fh[5] = {0.50f, 0.70f, 0.85f, 0.65f, 0.45f};
    for (int i = 0; i < 5; i++)
    {
      flames.push_back({makeTriangle(0.22f, fh[i], whiteTex, vec3(1.0f, 0.45f, 0.10f)), vec3(cx + fx[i], -0.94f, fz + 0.02f), i * 1.3f, 7.0f + i});
      flames.push_back({makeTriangle(0.12f, fh[i] * 0.6f, whiteTex, vec3(1.0f, 0.85f, 0.20f)), vec3(cx + fx[i], -0.94f, fz + 0.025f), i * 1.3f + 0.5f, 9.0f + i});
    }

    // على الرف: شمعدانين + مزهريتين
    float mantelY = 0.58f;
    addCandlestick(cx - 1.0f, mantelY, cz + 0.08f, 0.30f);
    addCandlestick(cx + 1.0f, mantelY, cz + 0.08f, 0.30f);
    addVase(cx - 0.70f, mantelY, cz + 0.08f, vec3(0.92f, 0.92f, 0.90f), true);
    addVase(cx + 0.70f, mantelY, cz + 0.08f, vec3(0.08f, 0.08f, 0.09f), false);
  }

  // ===== 3) طاولة صغيرة مائلة شوي نحو نص الغرفة (عليها الحوض) =====
  // منبني الطاولة والحوض حوالين نقطة الأصل، وبعدين منحركهم كلهم بمصفوفة وحدة (tankM)
  mat4 tankM = translate(mat4(1.0f), vec3(-3.2f, 0.0f, -4.3f));
  tankM = rotate(tankM, radians(25.0f), vec3(0.0f, 1.0f, 0.0f)); // ميلة خفيفة، مو بالزاوية
  auto addLocalBox = [&](float cx, float cy, float cz, float w, float h, float d, GLuint &tex, vec3 color)
  {
    std::vector<PolygonWithTexture> bx = makeBox(cx, cy, cz, w, h, d, tex, color);
    for (auto &f : bx)
      f.transformation(tankM);
    decor.insert(decor.end(), bx.begin(), bx.end());
  };
  {
    vec3 woodDark(0.45f, 0.30f, 0.20f);
    addLocalBox(0.0f, -0.35f, 0.0f, 1.5f, 0.08f, 1.1f, woodTex, woodDark); // السطح (علوه y = -0.31)
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        addLocalBox(sx * 0.65f, -0.695f, sz * 0.45f, 0.10f, 0.61f, 0.10f, woodTex, woodDark); // الأرجل
  }

  // ===== 4) نبتة كبيرة على الجدار اليسار بعد الكونسول =====
  {
    float px = -4.05f, pz = -0.4f;
    vec3 potCol(0.80f, 0.72f, 0.58f);
    // الوعاء المضلّع: أسطوانة بـ 12 ضلع (كل ضلع بدرجة إضاءة مختلفة) + حصى بيضا فوقه
    addPart(makeCylinder(px, -1.0f + 0.23f, pz, 0.22f, 0.32f, 0.46f, 12, whiteTex, potCol));
    addPart(makeCylinder(px, -1.0f + 0.455f, pz, 0.30f, 0.30f, 0.02f, 12, whiteTex, vec3(0.92f)));

    // 9 أوراق: لكل واحدة اتجاه (yaw) وميل (pitch) وطول ساق مختلف
    float baseY = -1.0f + 0.46f;
    float yaw[9] = {10, 50, 85, 25, 70, -5, 40, 60, 95};
    float pitch[9] = {10, 18, 26, 14, 32, 22, 12, 28, 20};
    float stemL[9] = {0.70f, 0.95f, 1.20f, 0.85f, 1.10f, 1.30f, 0.65f, 1.00f, 0.80f};
    float bladeL[9] = {0.75f, 0.80f, 0.70f, 0.65f, 0.80f, 0.70f, 0.60f, 0.75f, 0.65f};
    float droop[9] = {10, 15, 25, 8, 20, 12, 18, 22, 14};
    for (int i = 0; i < 9; i++)
    {
      mat4 base = translate(mat4(1.0f), vec3(px, baseY, pz));
      base = rotate(base, radians(yaw[i]), vec3(0.0f, 1.0f, 0.0f));
      base = rotate(base, radians(pitch[i]), vec3(1.0f, 0.0f, 0.0f));
      float g = 0.10f + 0.04f * (i % 3);
      addPart(makeLeaf(stemL[i], bladeL[i], 0.19f, whiteTex, vec3(0.20f, 0.35f, 0.15f), vec3(g, 0.28f + 0.03f * (i % 2), 0.12f), base, droop[i]));
    }
  }

  // ===== 5) حوض سمك زجاجي فوق الطاولة الصغيرة =====
  // القاعدة والرمل والصخور والنباتات والإطار: معتمة (بتنرسم مع الديكور).
  // الماء والزجاج: شفافين (بينرسموا آخر شي بالـ loop بعد كل الأشياء المعتمة).
  const float tankBase = -0.31f;               // علو سطح الطاولة الصغيرة
  const float TANK_W = 1.10f, TANK_D = 0.80f;  // عرض وعمق الحوض
  const float GLASS_H = 0.50f;                 // ارتفاع الزجاج
  const float WATER_H = 0.40f;                 // ارتفاع الماء (من القاعدة)
  const float baseTop = tankBase + 0.04f;      // سطح قاعدة الحوض
  const float gravelTop = baseTop + 0.06f;     // سطح الرمل
  const float waterTop = baseTop + WATER_H;    // سطح الماء
  // لون الستائر (بدّليه براحتك):
  //   أخضر مريمية: vec3(0.50f, 0.58f, 0.45f)  |  كاراميل: vec3(0.72f, 0.52f, 0.34f)  |  كريمي: vec3(0.90f, 0.85f, 0.72f)  |  رمادي دافي: vec3(0.55f, 0.52f, 0.48f)
  vec3 curtainColor(0.50f, 0.58f, 0.45f);
  std::vector<PolygonWithTexture> waterFaces, glassFaces, glassShine;
  std::vector<Fish> fishes;
  {
    vec3 rimWood(0.45f, 0.30f, 0.20f);
    auto addLocalBoxTo = [&](std::vector<PolygonWithTexture> &dst, float cx, float cy, float cz, float w, float h, float d, GLuint &tex, vec3 color)
    {
      std::vector<PolygonWithTexture> bx = makeBox(cx, cy, cz, w, h, d, tex, color);
      for (auto &f : bx)
        f.transformation(tankM);
      dst.insert(dst.end(), bx.begin(), bx.end());
    };
    auto addLocalParts = [&](std::vector<PolygonWithTexture> &dst, std::vector<PolygonWithTexture> parts)
    {
      for (auto &f : parts)
        f.transformation(tankM);
      dst.insert(dst.end(), parts.begin(), parts.end());
    };

    // القاعدة الخشبية + الرمل
    addLocalBox(0.0f, tankBase + 0.02f, 0.0f, TANK_W + 0.10f, 0.04f, TANK_D + 0.10f, woodTex, rimWood);
    addLocalBox(0.0f, baseTop + 0.03f, 0.0f, TANK_W - 0.04f, 0.06f, TANK_D - 0.04f, whiteTex, vec3(0.82f, 0.72f, 0.52f));

    // صخور (أسطوانات مضلّعة قليلة الارتفاع)
    addLocalParts(decor, makeCylinder(-0.30f, gravelTop + 0.035f, 0.12f, 0.09f, 0.06f, 0.07f, 7, whiteTex, vec3(0.45f, 0.45f, 0.48f)));
    addLocalParts(decor, makeCylinder(0.35f, gravelTop + 0.03f, -0.15f, 0.08f, 0.05f, 0.06f, 7, whiteTex, vec3(0.52f, 0.50f, 0.50f)));
    addLocalParts(decor, makeCylinder(0.05f, gravelTop + 0.025f, -0.22f, 0.06f, 0.04f, 0.05f, 6, whiteTex, vec3(0.40f, 0.40f, 0.44f)));

    // نباتات مائية (مخاريط خضرا)
    float plantX[5] = {-0.42f, -0.37f, 0.42f, 0.46f, 0.0f};
    float plantZ[5] = {-0.10f, -0.17f, 0.15f, 0.09f, -0.30f};
    float plantH[5] = {0.30f, 0.22f, 0.34f, 0.24f, 0.28f};
    for (int i = 0; i < 5; i++)
      addLocalParts(decor, makeCylinder(plantX[i], gravelTop + plantH[i] / 2, plantZ[i], 0.035f, 0.0f, plantH[i], 6, whiteTex, vec3(0.12f + 0.04f * (i % 2), 0.45f, 0.20f)));

    // الإطار الخشبي: 4 أعمدة بالزوايا + إطار علوي رفيع
    float topY = baseTop + GLASS_H;
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        addLocalBox(sx * (TANK_W / 2), baseTop + GLASS_H / 2, sz * (TANK_D / 2), 0.035f, GLASS_H, 0.035f, woodTex, rimWood);
    addLocalBox(0.0f, topY + 0.012f, TANK_D / 2, TANK_W + 0.04f, 0.025f, 0.04f, woodTex, rimWood);
    addLocalBox(0.0f, topY + 0.012f, -TANK_D / 2, TANK_W + 0.04f, 0.025f, 0.04f, woodTex, rimWood);
    addLocalBox(TANK_W / 2, topY + 0.012f, 0.0f, 0.04f, 0.025f, TANK_D, woodTex, rimWood);
    addLocalBox(-TANK_W / 2, topY + 0.012f, 0.0f, 0.04f, 0.025f, TANK_D, woodTex, rimWood);

    // الماء: صندوق شفاف أزرق بيبدأ من سطح الرمل
    float waterH2 = waterTop - gravelTop;
    addLocalBoxTo(waterFaces, 0.0f, gravelTop + waterH2 / 2, 0.0f, TANK_W - 0.06f, waterH2, TANK_D - 0.06f, whiteTex, vec3(0.35f, 0.70f, 0.92f));

    // الزجاج: 4 ألواح رفيعة جداً
    vec3 glassCol(0.85f, 0.95f, 1.0f);
    float gy = baseTop + GLASS_H / 2;
    addLocalBoxTo(glassFaces, 0.0f, gy, TANK_D / 2, TANK_W, GLASS_H, 0.01f, whiteTex, glassCol);
    addLocalBoxTo(glassFaces, 0.0f, gy, -TANK_D / 2, TANK_W, GLASS_H, 0.01f, whiteTex, glassCol);
    addLocalBoxTo(glassFaces, TANK_W / 2, gy, 0.0f, 0.01f, GLASS_H, TANK_D, whiteTex, glassCol);
    addLocalBoxTo(glassFaces, -TANK_W / 2, gy, 0.0f, 0.01f, GLASS_H, TANK_D, whiteTex, glassCol);

    // لمعة زجاج: خطّين بيضا رفيعين على الزجاج الأمامي
    float zf = TANK_D / 2 + 0.007f;
    PolygonWithTexture s1 = makeQuad(vec3(-0.40f, baseTop + 0.05f, zf), vec3(-0.34f, baseTop + 0.05f, zf), vec3(-0.34f, baseTop + 0.42f, zf), vec3(-0.40f, baseTop + 0.42f, zf), whiteTex, vec3(1.0f));
    PolygonWithTexture s2 = makeQuad(vec3(-0.29f, baseTop + 0.05f, zf), vec3(-0.26f, baseTop + 0.05f, zf), vec3(-0.26f, baseTop + 0.30f, zf), vec3(-0.29f, baseTop + 0.30f, zf), whiteTex, vec3(1.0f));
    s1.transformation(tankM);
    s2.transformation(tankM);
    glassShine.push_back(s1);
    glassShine.push_back(s2);

    // 5 أسماك: مسارات بيضاوية جوا الماء، كل وحدة على ارتفاع مختلف
    float yMin = gravelTop + 0.10f, yMax = waterTop - 0.08f;
    vec2 c(0.0f, 0.0f);
    fishes.push_back(makeFish(whiteTex, vec3(1.0f, 0.50f, 0.10f), c, vec2(0.34f, 0.20f), 0.6f, 0.0f, 1.0f, yMin + (yMax - yMin) * 0.80f));
    fishes.push_back(makeFish(whiteTex, vec3(1.0f, 0.85f, 0.20f), c, vec2(0.26f, 0.15f), 0.9f, 2.0f, -1.0f, yMin + (yMax - yMin) * 0.45f));
    fishes.push_back(makeFish(whiteTex, vec3(0.90f, 0.15f, 0.15f), vec2(-0.10f, 0.0f), vec2(0.22f, 0.12f), 1.1f, 4.0f, 1.0f, yMin + (yMax - yMin) * 0.10f));
    fishes.push_back(makeFish(whiteTex, vec3(1.0f, 0.70f, 0.80f), vec2(0.10f, 0.0f), vec2(0.20f, 0.10f), 0.8f, 1.0f, -1.0f, yMin + (yMax - yMin) * 0.65f));
    fishes.push_back(makeFish(whiteTex, vec3(0.60f, 0.30f, 0.80f), c, vec2(0.12f, 0.08f), 1.3f, 3.0f, 1.0f, yMin + (yMax - yMin) * 0.30f));
  }

  // ===== 6) شبابيك + ستائر سماوية على الجدار اليمين =====
  {
    vec3 frameWood(0.45f, 0.30f, 0.20f);
    float yBot = -0.95f, yTop = 2.95f;
    // wallX: مكان الجدار، dir: اتجاه الغرفة، zc: مركز الستائر، gap: عرض الفتحة بالنص، panelW: عرض كل ستارة
    auto addCurtains = [&](float wallX, float dir, float zc, float gap, float panelW)
    {
      // الشباك: زجاج سماوي فاتح + إطار خشبي رفيع + صليب
      float gw = gap + 0.2f, wb = 0.30f, wt = 1.95f;
      float gx = wallX + dir * 0.005f;
      addQuad(vec3(gx, wb, zc - gw / 2), vec3(gx, wb, zc + gw / 2), vec3(gx, wt, zc + gw / 2), vec3(gx, wt, zc - gw / 2), vec3(0.72f, 0.88f, 1.0f));
      float fx = wallX + dir * 0.012f;
      addBox(fx, wb, zc, 0.02f, 0.06f, gw + 0.12f, woodTex, frameWood);
      addBox(fx, wt, zc, 0.02f, 0.06f, gw + 0.12f, woodTex, frameWood);
      addBox(fx, (wb + wt) / 2, zc - gw / 2, 0.02f, wt - wb, 0.06f, woodTex, frameWood);
      addBox(fx, (wb + wt) / 2, zc + gw / 2, 0.02f, wt - wb, 0.06f, woodTex, frameWood);
      addBox(fx, (wb + wt) / 2, zc, 0.02f, wt - wb, 0.03f, woodTex, frameWood);
      // القضيب + الستارتين (واحدة يسار الفتحة وواحدة يمينها)
      float span = gap + 2 * panelW;
      addBox(wallX + dir * 0.14f, yTop + 0.05f, zc, 0.04f, 0.04f, span + 0.2f, woodTex, frameWood);
      addPart(makeCurtainPanel(wallX, dir, zc - gap / 2 - panelW, zc - gap / 2, yBot, yTop, 6, 0.08f, whiteTex, curtainColor));
      addPart(makeCurtainPanel(wallX, dir, zc + gap / 2, zc + gap / 2 + panelW, yBot, yTop, 6, 0.08f, whiteTex, curtainColor));
    };
    addCurtains(+ROOM_X, -1.0f, -2.4f, 1.0f, 0.6f); // الجدار اليمين
  }

  // ---------- سطح الطاولة: قماش واحد مدوّر الزوايا + خطوط رفيعة تحدد الأقسام الثلاثة ----------
  // لون القماش (بدّليه براحتك):
  //   أخضر غامق: vec3(0.06f, 0.28f, 0.14f)   |   أزرق غامق: vec3(0.08f, 0.14f, 0.32f)   |   عنابي: vec3(0.36f, 0.07f, 0.11f)
  vec3 TABLE_COLOR = vec3(0.06f, 0.28f, 0.14f);
  // لون الخطوط: نفس لون القماش بس أفتح، فبتبين كأنها خياطة بالقماش
  vec3 lineColor = vec3(std::min(TABLE_COLOR.x * 2.0f, 1.0f), std::min(TABLE_COLOR.y * 2.0f, 1.0f), std::min(TABLE_COLOR.z * 2.0f, 1.0f));

  // القماش: مضلع وحد مدوّر الزوايا (متناسق مع استدارة الطاولة 0.40 ناقص عرض الإطار 0.12)
  // وتحته حافة أغمق شوي (متل حافة الطاولات الحقيقية)
  PolygonWithTexture feltEdge = makeRoundedFlat(0.0f, 0.0f, 3.80f, 2.80f, 0.004f, 0.295f, whiteTex, TABLE_COLOR * 0.45f);
  PolygonWithTexture felt = makeRoundedFlat(0.0f, 0.0f, 3.76f, 2.76f, 0.008f, 0.28f, whiteTex, TABLE_COLOR);
  // خطّين رفيعين بيفصلوا: قسم الخصم | منطقة اللعب | مخزون اللاعب
  std::vector<PolygonWithTexture> zoneLines;
  zoneLines.push_back(makeFlatRect(0.0f, -0.5f, 3.76f, 0.014f, 0.012f, whiteTex, lineColor));
  zoneLines.push_back(makeFlatRect(0.0f, 0.5f, 3.76f, 0.014f, 0.012f, whiteTex, lineColor));

  // ---------- البطاقات ----------
  unsigned int cardTex[5];
  for (int i = 0; i < 5; i++)
    cardTex[i] = loadTextureAuto("./textures/card" + std::to_string(i + 1));

  std::vector<Card> hand;
  for (int i = 0; i < 5; i++)
    hand.push_back(Card(cardTex[i], whiteTex, vec3(cardSlotX(i), 0.02f, HAND_Z)));
  std::vector<Card> played;
  srand((unsigned int)time(0));

  // ---------- الكاميرتان ----------
  mat4 view1 = lookAt(vec3(0.0f, 3.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f)); // فوق الطاولة مباشرة وقريبة منها
  mat4 view2 = lookAt(vec3(0.0f, 2.25f, 3.95f), vec3(0.0f, 0.05f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

  ourShader.use();
  mat4 projection = perspective(radians(58.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
  ourShader.setMat4("projection", projection);

  float standAmount = 0.0f; // 0 = البطاقات مسطحة، 1 = واقفة
  float lastFrame = 0.0f;

  while (!glfwWindowShouldClose(window))
  {
    // الزمن بين frame وframe
    float now = (float)glfwGetTime();
    float dt = now - lastFrame;
    lastFrame = now;
    deltaTime = dt;

    processInput(window);

    // كاميرا 3: منحبسها جوا الغرفة
    if (currentCamera == 3)
    {
      camera.Position.x = std::max(-ROOM_X + 0.3f, std::min(ROOM_X - 0.3f, camera.Position.x));
      camera.Position.y = std::max(-0.6f, std::min(3.0f, camera.Position.y));
      camera.Position.z = std::max(ROOM_BACK_Z + 0.3f, std::min(ROOM_FRONT_Z - 0.3f, camera.Position.z));
    }
    // الوقوف/التسطيح التدريجي حسب الكاميرا
    float standTarget = (currentCamera != 1) ? 1.0f : 0.0f;
    float step = dt / STAND_DURATION;
    if (standAmount < standTarget)
      standAmount = std::min(standAmount + step, standTarget);
    else
      standAmount = std::max(standAmount - step, standTarget);

    // زر G: ننقل بطاقة عشوائية من المخزون لمنطقة اللعب
    if (gTriggered)
    {
      gTriggered = false;
      if (!hand.empty())
      {
        int idx = rand() % hand.size();
        Card card = hand[idx];
        hand.erase(hand.begin() + idx);
        int slot = (int)played.size();
        card.moveTo(vec3(cardSlotX(slot), 0.02f, PLAYED_Z));
        played.push_back(card);
      }
    }

    // تحديث حركة البطاقات
    for (auto &c : hand)
      c.update(dt);
    for (auto &c : played)
      c.update(dt);

    // كاميرا 1 (فوق الطاولة): خلفية داكنة بدون غرفة. كاميرا 2: لون الخلفية العادي
    if (currentCamera == 1)
      glClearColor(0.04f, 0.04f, 0.05f, 1.0f);
    else
      glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 view = (currentCamera == 1) ? view1 : (currentCamera == 2) ? view2 : camera.GetViewMatrix();
    ourShader.setMat4("view", view);

    // الغرفة (أرضية، جدران، بوستر، ديكور) بتنرسم بس بكاميرا اللاعب. كاميرا 1 بتشوف الطاولة لحالها
    if (currentCamera != 1)
    {
      for (auto &face : room)
        face.draw(ourShader);
      poster.draw(ourShader);
      for (auto &face : decor)
        face.draw(ourShader);

      // لهب المدفأة: بيتمدد ويتقلص مع الزمن
      for (auto &f : flames)
      {
        mat4 m = translate(mat4(1.0f), f.base);
        m = scale(m, vec3(1.0f, 1.0f + 0.18f * std::sin(now * f.speed + f.phase), 1.0f));
        f.poly.transformation(m);
        f.poly.draw(ourShader);
      }

      // ساعة الحائط: العقارب بالوقت الحقيقي
      time_t tt = time(0);
      tm *lt = localtime(&tt);
      float sec = (float)lt->tm_sec;
      float mn = lt->tm_min + sec / 60.0f;
      float hr = (lt->tm_hour % 12) + mn / 60.0f;
      vec3 zAxis(0.0f, 0.0f, 1.0f);
      hourHand.transformation(rotate(translate(clockBase, vec3(0, 0, 0.025f)), radians(-hr * 30.0f), zAxis));
      minuteHand.transformation(rotate(translate(clockBase, vec3(0, 0, 0.035f)), radians(-mn * 6.0f), zAxis));
      secondHand.transformation(rotate(translate(clockBase, vec3(0, 0, 0.045f)), radians(-sec * 6.0f), zAxis));
      clockRim.draw(ourShader);
      clockFace.draw(ourShader);
      hourHand.draw(ourShader);
      minuteHand.draw(ourShader);
      secondHand.draw(ourShader);

      // الأسماك: بتسبح جوا الماء على مسار بيضاوي، جسمها جانبي وذيلها بيتحرك
      for (auto &f : fishes)
      {
        float a = f.dir * (now * f.speed) + f.phase;
        float fxp = f.center.x + f.radius.x * std::cos(a);
        float fzp = f.center.y + f.radius.y * std::sin(a);
        float fyp = f.yBase + 0.025f * std::sin(now * 1.5f + f.phase * 2.0f); // تموّج بسيط بالارتفاع
        // اتجاه الحركة = مشتقة المسار، منحوّله لزاوية دوران حول Y (رأس السمكة باتجاه +x محلياً)
        float heading = std::atan2(-f.dir * f.radius.y * std::cos(a), -f.dir * f.radius.x * std::sin(a));
        mat4 m = translate(tankM, vec3(fxp, fyp, fzp));
        m = rotate(m, heading, vec3(0.0f, 1.0f, 0.0f));
        m = scale(m, vec3(0.8f)); // نصغّرها لتناسب الحوض

        f.body.transformation(scale(m, vec3(0.15f, 0.075f, 1.0f))); // الجسم الجانبي (قرص ممدود)
        // سماكة البطن: قرص أفقي رفيع
        mat4 bm = rotate(m, radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));
        f.belly.transformation(scale(bm, vec3(0.15f, 0.035f, 1.0f)));
        // الذيل: بيتحرك يمين ويسار
        float wag = std::sin(now * 9.0f + f.phase) * 0.45f;
        mat4 tm2 = translate(m, vec3(-0.13f, 0.0f, 0.0f));
        tm2 = rotate(tm2, wag, vec3(0.0f, 1.0f, 0.0f));
        f.tail.transformation(tm2);

        f.body.draw(ourShader);
        f.belly.draw(ourShader);
        f.tail.draw(ourShader);
        // عين على كل جهة
        f.eye.transformation(translate(m, vec3(0.09f, 0.03f, 0.012f)));
        f.eye.draw(ourShader);
        f.eye.transformation(translate(m, vec3(0.09f, 0.03f, -0.012f)));
        f.eye.draw(ourShader);
      }
    }
    for (auto &face : table)
      face.draw(ourShader);
    feltEdge.draw(ourShader);
    felt.draw(ourShader);
    for (auto &line : zoneLines)
      line.draw(ourShader);

    for (auto &card : hand)
      card.draw(ourShader, standAmount);
    for (auto &card : played)
      card.draw(ourShader, standAmount);

    // ---- الأشياء الشفافة (ماء + زجاج الحوض): آخر شي، بعد كل الأشياء المعتمة ----
    // الدمج بيستخدم قيمة شفافية ثابتة (glBlendColor) فما بدنا نعدّل الشيدر
    if (currentCamera != 1)
    {
      glEnable(GL_BLEND);
      glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
      glDepthMask(GL_FALSE); // الشفاف بيتدمج مع اللي وراه بدون ما يحجبه

      glBlendColor(0.0f, 0.0f, 0.0f, 0.30f); // الماء
      for (auto &f : waterFaces)
        f.draw(ourShader);
      glBlendColor(0.0f, 0.0f, 0.0f, 0.14f); // الزجاج
      for (auto &f : glassFaces)
        f.draw(ourShader);
      glBlendColor(0.0f, 0.0f, 0.0f, 0.40f); // لمعة الزجاج
      for (auto &f : glassShine)
        f.draw(ourShader);

      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window)
{
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    currentCamera = 1;
  if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    currentCamera = 2;
  if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    currentCamera = 3;

  // مؤشر الماوس: مقفول بكاميرا 3 فقط (متل المحاضرة)، وحر بالباقي
  static int prevCamera = 1;
  if (currentCamera != prevCamera)
  {
    glfwSetInputMode(window, GLFW_CURSOR, currentCamera == 3 ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    firstMouse = true;
    prevCamera = currentCamera;
  }
  if (currentCamera == 3)
  {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      camera.ProcessKeyboard(RIGHT, deltaTime);
  }

  // زر G: نفعّل gTriggered فقط لما الزر ينتقل من "مش مضغوط" إلى "مضغوط"
  bool gDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
  if (gDown && !gKeyWasDown)
    gTriggered = true;
  gKeyWasDown = gDown;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
  if (currentCamera != 3)
    return;
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (firstMouse)
  {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos; // معكوس لأنو y بيزيد لتحت

  lastX = xpos;
  lastY = ypos;

  camera.ProcessMouseMovement(xoffset, yoffset);
}