/*****
 * drawpath3.cc
 *
 * Stores a path3 that has been added to a picture.
 *****/

#include "drawpath3.h"
#include "drawsurface.h"
#include "material.h"

#ifdef HAVE_GL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

namespace camp {

using vm::array;
using namespace prc;
  
#ifdef HAVE_GL
using gl::modelView;

BezierCurve drawPath3::R;
#endif

bool drawPath3::write(prcfile *out, unsigned int *, double, groupsmap&)
{
  Int n=g.length();
  if(n == 0 || invisible)
    return true;

  if(straight) {
    triple *controls=new(UseGC) triple[n+1];
    for(Int i=0; i <= n; ++i)
      controls[i]=g.point(i);
    
    out->addLine(n+1,controls,color);
  } else {
    int m=3*n+1;
    triple *controls=new(UseGC) triple[m];
    controls[0]=g.point((Int) 0);
    controls[1]=g.postcontrol((Int) 0);
    size_t k=1;
    for(Int i=1; i < n; ++i) {
      controls[++k]=g.precontrol(i);
      controls[++k]=g.point(i);
      controls[++k]=g.postcontrol(i);
    }
    controls[++k]=g.precontrol(n);
    controls[++k]=g.point(n);
    out->addBezierCurve(m,controls,color);
  }
  
  return true;
}

void drawPath3::render(double size2, const triple& b, const triple& B,
                       double perspective, bool transparent)
{
#ifdef HAVE_GL
  Int n=g.length();
  if(n == 0 || invisible || ((color.A < 1.0) ^ transparent))
    return;

  const bool billboard=interaction == BILLBOARD &&
    !settings::getSetting<bool>("offscreen");
  triple m,M;
  
  double f,F,s;
  if(perspective) {
    f=Min.getz()*perspective;
    F=Max.getz()*perspective;
    m=triple(min(f*b.getx(),F*b.getx()),min(f*b.gety(),F*b.gety()),b.getz());
    M=triple(max(f*B.getx(),F*B.getx()),max(f*B.gety(),F*B.gety()),B.getz());
    s=max(f,F);
  } else {
    m=b;
    M=B;
    s=1.0;
  }
  
  const pair size3(s*(B.getx()-b.getx()),s*(B.gety()-b.gety()));
  
  bbox3 box(m,M);
  box.transform(modelView.Tinv);
  m=box.Min();
  M=box.Max();

  if(!billboard && (Max.getx() < m.getx() || Min.getx() > M.getx() ||
                    Max.gety() < m.gety() || Min.gety() > M.gety() ||
                    Max.getz() < m.getz() || Min.getz() > M.getz()))
    return;
  
  RGBAColour Black(0.0,0.0,0.0,color.A);
  setcolors(false,Black,Black,color,Black,1.0);
  
  if(billboard) {
    for(Int i=0; i < n; ++i) {
      triple controls[]={BB.transform(g.point(i)),BB.transform(g.postcontrol(i)),
                         BB.transform(g.precontrol(i+1)),
                         BB.transform(g.point(i+1))};
      R.queue(controls,straight,size3.length()/size2,m,M);
    }
  } else {
    BB.init(center);
    for(Int i=0; i < n; ++i) {
      triple controls[]={g.point(i),g.postcontrol(i),g.precontrol(i+1),
                         g.point(i+1)};
      R.queue(controls,straight,size3.length()/size2,m,M);
    }
  }
  R.draw();
#endif
}

drawElement *drawPath3::transformed(const double* t)
{
  return new drawPath3(t,this);
}
  
bool drawNurbsPath3::write(prcfile *out, unsigned int *, double, groupsmap&)
{
  if(invisible)
    return true;

  out->addCurve(degree,n,controls,knots,color,weights);
  
  return true;
}

// Approximate bounds by bounding box of control polyhedron.
void drawNurbsPath3::bounds(const double* t, bbox3& b)
{
  double x,y,z;
  double X,Y,Z;
  
  triple* Controls;
  if(t == NULL) Controls=controls;
  else {
    Controls=new triple[n];
    for(size_t i=0; i < n; i++)
      Controls[i]=t*controls[i];
  }
  
  boundstriples(x,y,z,X,Y,Z,n,Controls);
  
  b.add(x,y,z);
  b.add(X,Y,Z);
  
  if(t == NULL) {
    Min=triple(x,y,z);
    Max=triple(X,Y,Z);
  } else delete[] Controls;
}

drawElement *drawNurbsPath3::transformed(const double* t)
{
  return new drawNurbsPath3(t,this);
}

void drawNurbsPath3::ratio(const double* t, pair &b, double (*m)(double, double),
                           double, bool &first)
{
  triple* Controls;
  if(t == NULL) Controls=controls;
  else {
    Controls=new triple[n];
    for(size_t i=0; i < n; i++)
      Controls[i]=t*controls[i];
  }
  
  if(first) {
    first=false;
    triple v=Controls[0];
    b=pair(xratio(v),yratio(v));
  }
  
  double x=b.getx();
  double y=b.gety();
  for(size_t i=0; i < n; ++i) {
    triple v=Controls[i];
    x=m(x,xratio(v));
    y=m(y,yratio(v));
  }
  b=pair(x,y);
  
  if(t != NULL)
    delete[] Controls;
}

void drawNurbsPath3::displacement()
{
#ifdef HAVE_GL
  size_t nknots=degree+n+1;
  if(Controls == NULL) {
    Controls=new(UseGC)  GLfloat[(weights ? 4 : 3)*n];
    Knots=new(UseGC) GLfloat[nknots];
  }
  if(weights)
    for(size_t i=0; i < n; ++i)
      store(Controls+4*i,controls[i],weights[i]);
  else
    for(size_t i=0; i < n; ++i)
      store(Controls+3*i,controls[i]);
  
  for(size_t i=0; i < nknots; ++i)
    Knots[i]=knots[i];
#endif  
}

void drawNurbsPath3::render(double, const triple&, const triple&,
                            double, bool transparent)
{
#ifdef HAVE_GL
  if(invisible || ((color.A < 1.0) ^ transparent))
    return;
  
  GLfloat Diffuse[]={0.0,0.0,0.0,(GLfloat) color.A};
  glMaterialfv(GL_FRONT,GL_DIFFUSE,Diffuse);
  
  static GLfloat Black[]={0.0,0.0,0.0,1.0};
  glMaterialfv(GL_FRONT,GL_AMBIENT,Black);
    
  GLfloat Emissive[]={(GLfloat) color.R,(GLfloat) color.G,(GLfloat) color.B,
		      (GLfloat) color.A};
  glMaterialfv(GL_FRONT,GL_EMISSION,Emissive);
    
  glMaterialfv(GL_FRONT,GL_SPECULAR,Black);
  
  glMaterialf(GL_FRONT,GL_SHININESS,128.0);
#endif
}

bool drawPixel::write(prcfile *out, unsigned int *, double, groupsmap&)
{
  if(invisible)
    return true;

  out->addPoint(v,color,width);
  
  return true;
}
  
void drawPixel::render(double size2, const triple& b, const triple& B,
                       double perspective, bool transparent) 
{
#ifdef HAVE_GL
  if(invisible || ((color.A < 1.0) ^ transparent)) return;
  triple m,M;
  
  double f,F,s;
  if(perspective) {
    f=Min.getz()*perspective;
    F=Max.getz()*perspective;
    m=triple(min(f*b.getx(),F*b.getx()),min(f*b.gety(),F*b.gety()),b.getz());
    M=triple(max(f*B.getx(),F*B.getx()),max(f*B.gety(),F*B.gety()),B.getz());
    s=max(f,F);
  } else {
    m=b;
    M=B;
    s=1.0;
  }
  
  const pair size3(s*(B.getx()-b.getx()),s*(B.gety()-b.gety()));
  
  bbox3 box(m,M);
  box.transform(modelView.Tinv);
  m=box.Min();
  M=box.Max();

  if((Max.getx() < m.getx() || Min.getx() > M.getx() ||
      Max.gety() < m.gety() || Min.gety() > M.gety() ||
      Max.getz() < m.getz() || Min.getz() > M.getz()))
    return;
  
  RGBAColour Black(0.0,0.0,0.0,color.A);
  setcolors(false,color,Black,color,Black,1.0);
  
  glPointSize(1.0+width);
  R.draw(v);
  glPointSize(1.0);
#endif
}

drawElement *drawPixel::transformed(const double* t)
{
  return new drawPixel(t*v,p,width,KEY);
}
  
} //namespace camp
