// TODO: Check length vs. length^2

/*****
 * drawbezierpatch.cc
 *
 * Render a Bezier patch.
 *****/

#include "drawsurface.h"

namespace camp {

#ifdef HAVE_GL

static const double pixel=1.0; // Adaptive rendering constant.

extern const double Fuzz;
extern const double Fuzz2;

inline triple maxabs(triple u, triple v)
{
  return triple(max(fabs(u.getx()),fabs(v.getx())),
                max(fabs(u.gety()),fabs(v.gety())),
                max(fabs(u.getz()),fabs(v.getz())));
}

inline triple displacement1(const triple& z0, const triple& c0,
                            const triple& c1, const triple& z1)
{
  triple Z0=c0-z0;
  triple Q=unit(z1-z0);
  triple Z1=c1-z0;
  return maxabs(Z0-dot(Z0,Q)*Q,Z1-dot(Z1,Q)*Q);
}

// return the perpendicular displacement of a point z from the plane
// through u with unit normal n.
inline triple displacement2(const triple& z, const triple& u, const triple& n)
{
  triple Z=z-u;
  return n != triple(0,0,0) ? dot(Z,n)*n : Z;
}
  
// TODO: Simplify (see beziertriangle example)
inline triple displacement(const triple *controls)
{
  triple d;
  triple normal=unit(cross(controls[12]-controls[0],controls[3]-controls[0]));
       
  for(size_t i=1; i < 16; ++i) 
    d=maxabs(d,displacement2(controls[i],controls[0],normal));
      
   for(size_t i=0; i < 4; ++i)
    d=maxabs(d,displacement1(controls[4*i],controls[4*i+1],
                             controls[4*i+2],controls[4*i+3]));
  for(size_t i=0; i < 4; ++i)
    d=maxabs(d,displacement1(controls[i],controls[i+4],
                             controls[i+8],controls[i+12]));
  return d;
}
  
// Returns one-third of the first derivative of the Bezier curve defined by
// a,b,c,d at 0.
inline triple bezierP(triple a, triple b) {
  return b-a;
}

// Returns one-sixth of the second derivative of the Bezier curve defined
// by a,b,c,d at 0. 
inline triple bezierPP(triple a, triple b, triple c) {
  return a+c-2.0*b;
}

// Returns one-third of the third derivative of the Bezier curve defined by
// a,b,c,d.
inline triple bezierPPP(triple a, triple b, triple c, triple d) {
  return d-a+3.0*(b-c);
}

struct RenderPatch
{
  std::vector<GLfloat> buffer;
  std::vector<GLint> indices;
  triple u,v,w;
  GLuint nvertices;
  double cx,cy,cz;
  double epsilon;
  double res;
  bool billboard;
  
  void init(bool havebillboard, const triple& center) {
    const size_t nbuffer=10000;
    buffer.reserve(nbuffer);
    indices.reserve(nbuffer);
    nvertices=0;
    
    billboard=havebillboard;
    if(billboard) {
      cx=center.getx();
      cy=center.gety();
      cz=center.getz();

      gl::projection P=gl::camera(false);
      w=unit(P.camera-P.target);
      v=unit(perp(P.up,w));
      u=cross(v,w);
    }
  }
    
  void clear() {
    buffer.clear();
    indices.clear();
  }
  
// Store the vertex v and its normal vector n in the buffer.
  GLuint vertex(const triple& V, const triple& n) {
    if(billboard) {
      double x=V.getx()-cx;
      double y=V.gety()-cy;
      double z=V.getz()-cz;
      buffer.push_back(cx+u.getx()*x+v.getx()*y+w.getx()*z);
      buffer.push_back(cy+u.gety()*x+v.gety()*y+w.gety()*z);
      buffer.push_back(cz+u.getz()*x+v.getz()*y+w.getz()*z);
    } else {
      buffer.push_back(V.getx());
      buffer.push_back(V.gety());
      buffer.push_back(V.getz());
    }
    
    buffer.push_back(n.getx());
    buffer.push_back(n.gety());
    buffer.push_back(n.getz());
    
    return nvertices++;
  }
  
// Store the vertex v and its normal vector n and colour in the buffer.
  GLuint vertex(const triple& V, const triple& n, GLfloat *c) {
    int rc=vertex(V,n);
    buffer.push_back(c[0]);
    buffer.push_back(c[1]);
    buffer.push_back(c[2]);
    buffer.push_back(c[3]);
    return rc;
  }
  
  triple normal0(triple left3, triple left2, triple left1, triple middle,
                 triple right1, triple right2, triple right3) {
    //cout << "normal0 called." << endl;
    // Lots of repetition here.
    // TODO: Check if lp,rp,lpp,rpp should be manually inlined (i.e., is the
    // third order normal usually computed when normal0() is called?).
    triple lp=bezierP(middle,left1);
    triple rp=bezierP(middle,right1);
    triple lpp=bezierPP(middle,left1,left2);
    triple rpp=bezierPP(middle,right1,right2);
    triple n1=cross(rpp,lp)+cross(rp,lpp);
    if(abs2(n1) > epsilon) {
      return unit(n1);
    } else {
      triple lppp=bezierPPP(middle,left1,left2,left3);
      triple rppp=bezierPPP(middle,right1,right2,right3);
      triple n2= 9.0*cross(rpp,lpp)+
        3.0*(cross(rp,lppp)+cross(rppp,lp)+
             cross(rppp,lpp)+cross(rpp,lppp))+
        cross(rppp,lppp);
      return unit(n2);
    }
  }

  triple normal(triple left3, triple left2, triple left1, triple middle,
                triple right1, triple right2, triple right3) {
    triple bu=right1-middle;
    triple bv=left1-middle;
    triple n=triple(bu.gety()*bv.getz()-bu.getz()*bv.gety(),
                    bu.getz()*bv.getx()-bu.getx()*bv.getz(),
                    bu.getx()*bv.gety()-bu.gety()*bv.getx());
    return abs2(n) > epsilon ? unit(n) :
      normal0(left3,left2,left1,middle,right1,right2,right3);
  }

  void mesh(const triple *p, const GLuint *I)
  {
    // Draw the frame of the control points of a cubic Bezier mesh
    indices.push_back(I[0]);
    indices.push_back(I[1]);
    indices.push_back(I[2]);
    indices.push_back(I[0]);
    indices.push_back(I[2]);
    indices.push_back(I[3]);
  }
  
  struct Split3 {
    triple m0,m1,m2,m3,m4,m5;
    Split3(triple z0, triple c0, triple c1, triple z1) {
      m0=0.5*(z0+c0);
      m1=0.5*(c0+c1);
      m2=0.5*(c1+z1);
      m3=0.5*(m0+m1);
      m4=0.5*(m1+m2);
      m5=0.5*(m3+m4);
    }
  };

// Pi is the full precision value indexed by Ii.
// The 'flati' are flatness flags for each boundary.
  void render(const triple *p, int n,
              GLuint I0, GLuint I1, GLuint I2, GLuint I3,
              triple P0, triple P1, triple P2, triple P3,
              bool flat0, bool flat1, bool flat2, bool flat3,
              GLfloat *C0=NULL, GLfloat *C1=NULL, GLfloat *C2=NULL,
              GLfloat *C3=NULL)
  {
    // Uses a uniform partition
    // p points to an array of 16 triples.
    // Draw a Bezier triangle.
    // p is the set of control points for the Bezier triangle
    // n is the maximum number of iterations to compute
    triple d=displacement(p);

    // This involves fewer triangle computations at the end (since if the
    // surface is sufficiently flat, it just draws the sufficiently flat
    // triangle, rather than trying to properly utilize the already
    // computed values. 

    if(n == 0 || length(d) < res) { // If triangle is flat...
      GLuint I[]={I0,I1,I2,I3};
      mesh(p,I);
    } else { // Triangle is not flat

      Split3 c0(p[0],p[1],p[2],p[3]);
      Split3 c1(p[4],p[5],p[6],p[7]);
      Split3 c2(p[8],p[9],p[10],p[11]);
      Split3 c3(p[12],p[13],p[14],p[15]);

      Split3 c4(p[0],p[4],p[8],p[12]);
      Split3 c5(c0.m0,c1.m0,c2.m0,c3.m0);
      Split3 c6(c0.m3,c1.m3,c2.m3,c3.m3);
      Split3 c7(c0.m5,c1.m5,c2.m5,c3.m5);
      Split3 c8(c0.m4,c1.m4,c2.m4,c3.m4);
      Split3 c9(c0.m2,c1.m2,c2.m2,c3.m2);
      Split3 c10(p[3],p[7],p[11],p[15]);

      triple s0[]={p[0],c0.m0,c0.m3,c0.m5,c4.m0,c5.m0,c6.m0,c7.m0,
                   c4.m3,c5.m3,c6.m3,c7.m3,c4.m5,c5.m5,c6.m5,c7.m5};
      triple s1[]={c4.m5,c5.m5,c6.m5,c7.m5,c4.m4,c5.m4,c6.m4,c7.m4,
                   c4.m2,c5.m2,c6.m2,c7.m2,p[12],c3.m0,c3.m3,c3.m5};
      triple s2[]={c7.m5,c8.m5,c9.m5,c10.m5,c7.m4,c8.m4,c9.m4,c10.m4,
                   c7.m2,c8.m2,c9.m2,c10.m2,c3.m5,c3.m4,c3.m2,p[15]};
      triple s3[]={c0.m5,c0.m4,c0.m2,p[3],c7.m0,c8.m0,c9.m0,c10.m0,
                   c7.m3,c8.m3,c9.m3,c10.m3,c7.m5,c8.m5,c9.m5,c10.m5};
      --n;

      triple m0=s0[12];
      triple m1=s1[15];
      triple m2=s2[3];
      triple m3=s3[0];
      triple m4=s0[15];
      
      /*
      if(C0) {
        GLfloat c0[4],c1[4],c2[4];
        for(int i=0; i < 4; ++i) {
          c0[i]=0.5*(C1[i]+C2[i]);
          c1[i]=0.5*(C0[i]+C2[i]);
          c2[i]=0.5*(C0[i]+C1[i]);
        }
      
        GLuint i0=vertex(p0,normal(l300,r012,r021,r030,u201,u102,l030),c0);
        GLuint i1=vertex(p1,normal(r030,u201,u102,l030,l120,l210,l300),c1);
        GLuint i2=vertex(p2,normal(l030,l120,l210,l300,r012,r021,r030),c2);
          
        render(l,n,I0,i2,i1,P0,p2,p1,flat1,flat2,false,C0,c2,c1);
        render(r,n,i2,I1,i0,p2,P1,p0,flat1,false,flat3,c2,C1,c0);
        render(u,n,i1,i0,I2,p1,p0,P2,false,flat2,flat3,c1,c0,C2);
        render(c,n,i0,i1,i2,p0,p1,p2,false,false,false,c0,c1,c2);
      } else {
      */ {
        GLuint i0=vertex(m0,normal(s0[0],s0[4],s0[8],m0,s0[13],s0[14],s0[15]));
        GLuint i1=vertex(m1,normal(s1[12],s1[13],s1[14],m1,s1[11],s1[7],s1[3]));
        GLuint i2=vertex(m2,normal(s2[15],s2[11],s2[7],m2,s2[2],s2[1],s2[0]));
        GLuint i3=vertex(m3,normal(s3[3],s3[2],s3[1],m3,s3[4],s3[8],s3[12]));
        GLuint i4=vertex(m4,normal(s2[3],s2[2],s2[1],m4,s2[4],s2[8],s2[12]));
      render(s0,n,I0,i0,i4,i3,P0,m0,m4,m3,flat0,false,false,flat3);
      render(s1,n,i0,I1,i1,i4,m0,P1,m1,m4,flat0,flat1,false,false);
      render(s2,n,i4,i1,I2,i2,m4,m1,P2,m2,false,flat1,flat2,false);
      render(s3,n,i3,i4,i2,I3,m3,m4,m2,P3,false,false,flat2,flat3);
      }
    }
  }

// n is the maximum depth
  void render(const triple *p, double res, GLfloat *c0, int n) {
    this->res=res;

    triple p0=p[0];
    epsilon=0;
    for(int i=1; i < 16; ++i)
      epsilon=max(epsilon,abs2(p[i]-p0));
  
    epsilon *= Fuzz2;
    
    GLuint i0,i1,i2,i3;
    
    triple p3=p[3];
    triple p12=p[12];
    triple p15=p[15];

    if(c0) {
      /*
      GLfloat *c1=c0+4;
      GLfloat *c2=c0+8;
      GLfloat *c3=c0+12;
    
      i0=vertex(p0, normal(p3,p[2],p[1],p0,p[4],p[8],p12),c0);
      i1=vertex(p12,normal(p0,p[4],p[8],p12,p[13],p[14],p15),c1);
      i2=vertex(p15,normal(p12,p[13],p[14],p15,p[11],p[7],p3),c2);
      i3=vertex(p3,normal(p15,p[11],p[7],p3,p[2],p[1],p0),c3);
    
      if(n > 0)
        render(p,n,i0,i1,i2,i3,p0,p12,p15,p3,false,false,false,false,
        c0,c1,c2,c3);
      */
    } else {
      i0=vertex(p0,normal(p3,p[2],p[1],p0,p[4],p[8],p12));
      i1=vertex(p12,normal(p0,p[4],p[8],p12,p[13],p[14],p15));
      i2=vertex(p15,normal(p12,p[13],p[14],p15,p[11],p[7],p3));
      i3=vertex(p3,normal(p15,p[11],p[7],p3,p[2],p[1],p0));
    
      if(n > 0)
        render(p,n,i0,i1,i2,i3,p0,p12,p15,p3,false,false,false,false);
    }
    
    if(n == 0) {
      GLuint I[]={i0,i1,i2,i3};
      mesh(p,I);
    }
    
    size_t stride=(c0 ? 10 : 6)*sizeof(GL_FLOAT);

    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    if(c0) glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3,GL_FLOAT,stride,&buffer[0]);
    glNormalPointer(GL_FLOAT,stride,&buffer[3]);
    if(c0) glColorPointer(4,GL_FLOAT,stride,&buffer[6]);
    glDrawElements(GL_TRIANGLES,indices.size(),GL_UNSIGNED_INT,&indices[0]);
    if(c0) glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
  }
  
};

static RenderPatch R;

void bezierPatch(const triple *g, bool straight, double ratio,
                 bool havebillboard, triple center, GLfloat *colors)
{
  R.init(havebillboard,center);
  straight=false;
  R.render(g,pixel*ratio,colors,straight ? 0 : 8);
  R.clear();
}

#endif

} //namespace camp
