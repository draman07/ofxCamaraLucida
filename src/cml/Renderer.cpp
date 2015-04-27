#include "cml/Renderer.h"

namespace cml 
{
  Renderer::Renderer( 
      cml::Config config,
      OpticalDevice* proj, 
      OpticalDevice* depth )
      //OpticalDevice* rgb )
  {
    this->proj = proj;
    this->depth = depth;
    //this->rgb = rgb;

    _debug = false;
    _viewpoint = V_DEPTH;

    ofFbo::Settings s;
    s.width = config.tex_width;
    s.height = config.tex_height;
    s.numSamples = config.tex_nsamples;
    s.numColorbuffers	= 1;
    s.internalformat = GL_RGBA;

    fbo.allocate(s);

    //shader.load("camara_lucida/glsl/render");
    render_shader.init( shader );

    init_gl_scene_control();
  }

  Renderer::~Renderer()
  {
    dispose(); 
  }

  void Renderer::dispose()
  {
    proj = NULL;
    depth = NULL;
    //rgb = NULL;
  }

  void Renderer::render( 
      cml::Events *ev, 
      Mesh *mesh,
      ofTexture& depth_ftex,
      bool gpu, 
      bool wireframe )
  {

    // texture

    fbo.bind();
    //ofEnableAlphaBlending();  

    float w = fbo.getWidth();
    float h = fbo.getHeight();

    ofClear(0,1);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(1,1,1);

    ofNotifyEvent( ev->render_texture, ev->void_args );

    fbo.unbind();
    //ofDisableAlphaBlending(); 

    // gl init

    ofClear(0,1);

    // 3d

    ofEnableDepthTest();

    glPushAttrib( GL_POLYGON_BIT );
    if ( wireframe )
      glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); 
    else
    {
      glCullFace( GL_FRONT );
      glEnable( GL_CULL_FACE );
      glPolygonMode( GL_FRONT, GL_FILL );
    }

    ofPushStyle();
    ofSetColor( ofColor::white );

    ofViewport();
    gl_projection();	
    gl_viewpoint();

    //gl_scene_control();

    if ( _debug )
    {
      gl_scene_control();
      render_depth_CS();
      render_proj_CS();
      //render_rgb_CS();
      //ofDrawGrid( 3000.0f, 8.0f, false, false, true, false );
    }

    //ofEnableAlphaBlending();

    ofTexture render_tex = fbo.getTextureReference(0);

    ofPushMatrix();
    ofScale( -1., -1., 1. );	

    if ( gpu )
    {
      shader.begin();
      render_tex.bind();
      render_shader.update(shader, ((cml::DepthCamera*)depth), render_tex, depth_ftex);
      mesh->render();
      render_tex.unbind();
      shader.end();
    } 

    else
    {
      render_tex.bind();
      mesh->render();
      render_tex.unbind();
    } 
    ofPopMatrix();

    //ofDisableAlphaBlending(); 

    ofNotifyEvent( ev->render_3d, ev->void_args );

    // 2d hud

    glPopAttrib();//GL_POLYGON_BIT
    glDisable( GL_CULL_FACE );
    glPolygonMode( GL_FRONT, GL_FILL );
    ofDisableDepthTest();
    ofSetColor( ofColor::white );
    gl_ortho();
    ofNotifyEvent( ev->render_2d, ev->void_args );

    ofPopStyle();
  }

  // gl

  void Renderer::gl_ortho()
  {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(
        0, ofGetWidth(), ofGetHeight(),
        0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }

  void Renderer::gl_projection()
  {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    OpticalDevice* dev; 

    switch( _viewpoint )
    {
      case V_PROJ:
        dev = proj;        
        break;
      case V_DEPTH:
        dev = depth;
        break;
      //case V_RGB:
        //dev = rgb;
        //break;
    }

    OpticalDevice::Frustum& frustum = dev->gl_frustum();

    glFrustum( 
        frustum.left, frustum.right,
        frustum.bottom, frustum.top,
        frustum.near, frustum.far );

    //float* KK = dev->gl_projection_matrix();
    //glMultMatrixf( KK );
  }

  void Renderer::gl_viewpoint()
  {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    OpticalDevice* dev; 

    switch( _viewpoint )
    {
      case V_PROJ:
        dev = proj;        
        break;
      case V_DEPTH:
        dev = depth;
        break;
      //case V_RGB:
        //dev = rgb;
        //break;
    }

    ofVec3f& loc = dev->loc();
    ofVec3f& trg = dev->trg();
    ofVec3f& up = dev->up();

    gluLookAt(
      loc.x, loc.y, loc.z,
      trg.x, trg.y, trg.z,
      up.x,  up.y,  up.z 
    );
  }

  // scene control

  void Renderer::gl_scene_control()
  {
    glTranslatef( 0, 0, tZ );
    glTranslatef( rot_pivot.x, rot_pivot.y, rot_pivot.z );
    glRotatef( rotX, 1, 0, 0);
    glRotatef( rotY, 0, 1, 0);
    glRotatef( rotZ, 0, 0, 1);
    glTranslatef( -rot_pivot.x, -rot_pivot.y, -rot_pivot.z );
  }

  void Renderer::init_gl_scene_control()
  {
    //float *RT = proj->gl_modelview_matrix();
    float *RT = depth->gl_modelview_matrix();
    rot_pivot = ofVec3f( RT[12], RT[13], RT[14] );

    pmouse = ofVec2f();

    tZ_delta = -50.; //mm units
    rot_delta = -0.2;

    tZini = 0;
    rotXini = 0;
    rotYini = 0;
    rotZini = 0;

    reset_scene();
  }

  void Renderer::reset_scene()
  {
    tZ = tZini;
    rotX = rotXini;
    rotY = rotYini;
    rotZ = rotZini;
  }

  void Renderer::next_view()
  {
    ++_viewpoint;
    _viewpoint = _viewpoint == V_LENGTH ? 0 : _viewpoint;
  }

  void Renderer::prev_view()
  {
    --_viewpoint;
    _viewpoint = _viewpoint == -1 ? V_LENGTH-1 : _viewpoint;
  }

  void Renderer::render_depth_CS()
  {
    glPushMatrix();
    glMultMatrixf( depth->gl_modelview_matrix() );
    ofPushStyle();
    ofSetColor( ofColor::magenta );
    ofSetLineWidth(1);
    render_frustum(depth->gl_frustum());
    render_axis(50);
    ofPopStyle();
    glPopMatrix();
  }

  void Renderer::render_proj_CS()
  {
    glPushMatrix();
    glMultMatrixf( proj->gl_modelview_matrix() );

    ofPushStyle();
    ofSetLineWidth(1);
    ofSetColor( ofColor::yellow );

    render_frustum(proj->gl_frustum());
    render_axis(50);

    //ppal point
    //float len = 100;
    //ofSetColor( ofColor::yellow );
    //ofLine( 0,0,0, 0,0, len );
    //glBegin(GL_POINTS);
    //glVertex3f(0, 0, len);
    //glEnd();

    ofPopStyle();
    glPopMatrix();
  }

  //void Renderer::render_rgb_CS()
  //{
    //glPushMatrix();
    //glMultMatrixf(rgb->gl_modelview_matrix());
    //render_axis(50);
    //glPopMatrix();
  //}

  //see of3dUtils#ofDrawAxis
  void Renderer::render_axis(float size)
  {
    ofPushStyle();
    ofSetLineWidth(2);

    // draw x axis
    ofSetColor(ofColor::red);
    ofLine(0, 0, 0, size, 0, 0);

    // draw y axis
    ofSetColor(ofColor::green);
    ofLine(0, 0, 0, 0, size, 0);

    // draw z axis
    ofSetColor(ofColor::blue);
    ofLine(0, 0, 0, 0, 0, size);

    ofPopStyle();
  }

  void Renderer::render_frustum( OpticalDevice::Frustum& F )
  {
    float plane = F.near;

    //pyramid
    ofLine( 0,0,0, 
        F.left, F.top, plane );
    ofLine( 0,0,0, 
        F.right, F.top, plane );
    ofLine( 0,0,0, 
        F.left, F.bottom, plane );
    ofLine( 0,0,0, 
        F.right, F.bottom, plane );

    //plane
    ofLine( 
        F.left, F.bottom, plane, 
        F.left, F.top, plane );
    ofLine( 
        F.left, F.top, plane, 
        F.right, F.top, plane );
    ofLine( 
        F.right, F.top, plane, 
        F.right, F.bottom, plane );
    ofLine( 
        F.right, F.bottom, plane, 
        F.left, F.bottom, plane );
  };

  string Renderer::get_viewpoint_info()
  {
    switch( _viewpoint )
    {
      case V_PROJ:
        return "projector viewpoint";
        break;
      case V_DEPTH:
        return "depth camera viewpoint";
        break;
      //case V_RGB:
        //return "rgb camera viewpoint";
        //break;
      //case V_WORLD:
        //return "world viewpoint";
        //break;
      default:
        return "no viewpoint selected";
    }
  }

  // ui	

  void Renderer::mouseDragged(int x, int y, bool zoom)
  {
    if ( ! _debug) return;

    ofVec2f m = ofVec2f( x, y );
    ofVec2f dist = m - pmouse;

    if ( zoom )
    {
      tZ += -dist.y * tZ_delta;	
    }
    else
    {
      rotX += dist.y * rot_delta;
      rotY -= dist.x * rot_delta;
    }

    pmouse.set( x, y );
  }

  void Renderer::mousePressed(int x, int y)
  {
    pmouse.set( x, y );	
  }
};
