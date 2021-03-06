#include "ReZEL.h"
#include "Player.h"
#include "draw/material.h"
//#include "judge.h"
#include "draw/effects.h"
#include "draw/WarDraw.h"
#include "draw/OpenGLState.h"
#include "draw/mqoadapt.h"
#include "glstack.h"
#include "yssurf.h"
#include "glw/GLWchart.h"
#include "cmd.h"
#include "sqadapt.h"
#include "msg/GetCoverPointsMessage.h"
extern "C"{
#include <clib/c.h>
#include <clib/cfloat.h>
#include <clib/mathdef.h>
#include <clib/suf/sufbin.h>
#include <clib/suf/sufdraw.h>
#include <clib/suf/sufvbo.h>
#include <clib/avec3.h>
#include <clib/amat4.h>
#include <clib/aquat.h>
#include <clib/GL/gldraw.h>
#include <clib/GL/multitex.h>
#include <clib/wavsound.h>
#include <clib/zip/UnZip.h>
#include <clib/timemeas.h>
}
#include <gl/glext.h>
#include <assert.h>
#include <string.h>



/* color sequences */
extern const struct color_sequence cs_orangeburn, cs_shortburn;
#define DEFINE_COLSEQ(cnl,colrand,life) {COLOR32RGBA(0,0,0,0),numof(cnl),(cnl),(colrand),(life),1}
static const struct color_node cnl_orangeburn[] = {
	{0.1, COLOR32RGBA(255,255,191,0)},
	{0.1, COLOR32RGBA(255,255,191,255)},
	{0.15, COLOR32RGBA(255,255,31,191)},
	{0.45, COLOR32RGBA(255,127,31,95)},
	{0.3, COLOR32RGBA(255,31,0,63)},
};
const struct color_sequence cs_orangeburn = DEFINE_COLSEQ(cnl_orangeburn, (COLOR32)-1, 1.1);
static const struct color_node cnl_shortburn[] = {
	{0.1, COLOR32RGBA(255,255,191,255)},
	{0.15, COLOR32RGBA(255,255,31,191)},
	{0.25, COLOR32RGBA(255,127,31,0)},
};
const struct color_sequence cs_shortburn = DEFINE_COLSEQ(cnl_shortburn, (COLOR32)-1, 0.5);


double g_nlips_factor = 1.;
static int g_shader_enable = 0;

const double ReZEL::sufscale = 1./30000;


bool ReZEL::cull(Viewer &vw)const{
	double nf = nlipsFactor(vw);
	if(task == ReZEL::Undockque || vw.gc->cullFrustum(pos, .012 * nf))
		return true;
	double pixels = .008 * fabs(vw.gc->scale(pos)) * nf;
	if(pixels < 2)
		return true;
	return false;
}

/* NLIPS: Non-Linear Inverse Perspective Scrolling */
double ReZEL::nlipsFactor(Viewer &vw)const{
	double f = vw.fov * g_nlips_factor * 500. / vw.vp.m * 4. * ::sqrt((this->pos - vw.pos).len());
	return MAX(1., f);
}

Model *ReZEL::model = NULL;
Motion *ReZEL::motions[ReZEL::motionCount];

void ReZEL::getMotionTime(double (&motion_time)[numof(motions)], double (&motion_amplitude)[numof(motions)]){
	for(int i = 0; i < numof(motions); i++)
		motion_amplitude[i] = 1.;
	motion_time[0] = 10. * fwaverider;
	motion_amplitude[0] = fwaverider ? 1. : 0.;
	motion_time[1] = 10. * (1. - fwaverider) * (1. - fonfeet);
	motion_amplitude[1] = fwaverider != 1. ? 1. : 0.;
	motion_time[2] = 10. * (freload == 0. ? 1. - fweapon : 0.);
	motion_amplitude[2] = (1. - fwaverider) * (1. - fsabre) * (1. - coverFactor());
	motion_time[3] = 10. * (1. - fwaverider) * (1. - fsabre) * fweapon,
	motion_amplitude[3] = fwaverider != 1. ? 1. : 0.;
	motion_time[4] = fonfeet != 1.f ? (-twist * (1. - fwaverider) + 1.) * 10. : 10.;
	motion_amplitude[4] = 0.; // disable airtwist
	motion_time[5] = (-pitch * (1. - fwaverider) + 1.) * 10.;
	motion_amplitude[5] = 0.; // disable airpitch
	motion_time[6] = (1. - fwaverider) * (1. - fsabre) * (freload != 0. ? min(2. - 2. * freload / reloadTime, 2. * freload / reloadTime) * 20. + 10. : 0.);
	motion_amplitude[6] = fwaverider != 1. && fsabre != 1. ? 1. : 0.;
	motion_time[7] = (fsabre < 2. ? fsabre : 4. - fsabre) * 10.;
	motion_amplitude[7] = 1. - fwaverider;
	motion_time[8] = 10. * max(0., velo.len() / walkSpeed) * (walkphase * 8 + 1.);
	motion_amplitude[8] = fwaverider == 0. ? (1. - coverFactor()) * fonfeet : 0.;
	motion_time[9] = 10. * min(1., 1. - velo.len() / walkSpeed);
	motion_amplitude[9] = fwaverider == 0. ? (1. - coverFactor()) * fonfeet : 0.;
	motion_time[10] = 10. * rangein((aimdir[1] / (M_PI / 3.)) * (1. - fwaverider) + 1., 0., 2.);
	motion_amplitude[10] = (1. - fwaverider) * (1. - coverFactor());
	motion_time[11] = weapon == 0 && fwaverider == 0 ? 15. * (aimdir[0] / (M_PI / 2.) * (1. - fwaverider) + 1.) : 15.;
	motion_amplitude[11] = weapon == 0 ? (1. - fwaverider) * (1. - coverFactor()) : 0.;
	motion_time[12] = weapon == 1 && fwaverider == 0 ? 15. * (aimdir[0] / (M_PI / 2.) * (1. - fwaverider) + 1.) : 15.;
	motion_amplitude[12] = weapon == 1 && fwaverider == 0. ? 1. : 0.;
	motion_time[13] = weapon == 0 ? 10. * coverRight : 0;
	motion_amplitude[13] = (1. - fwaverider) * (weapon == 0 ? coverFactor() : 0.);
	motion_time[14] = weapon == 0 ? 10. * rangein(aimdir[1] / (M_PI / 4.) + 1., 0., 2.) : 0;
	motion_amplitude[14] = (weapon == 0 ? (1. - fwaverider) * max(0, coverRight - 1.) : 0.);
	motion_time[15] = weapon == 0 ? 10. * rangein(aimdir[0] / (M_PI / 4.) + 1., 0., 2.) : 0;
	motion_amplitude[15] = (weapon == 0 ? (1. - fwaverider) * max(0, coverRight - 1.) : 0.);
}

ReZEL::MotionPoseSet &ReZEL::motionInterpolate(){
	double motion_time[numof(motions)];
	double motion_amplitude[numof(motions)];
	getMotionTime(motion_time, motion_amplitude);
	double vectime[numof(motions)];
	double vecamp[numof(motions)];
	Motion* vecmotions[numof(motions)];

	if(memcmp(motion_time, this->motion_time, sizeof motion_time) || memcmp(motion_amplitude, this->motion_amplitude, sizeof motion_amplitude)){
		memcpy(this->motion_time, motion_time, sizeof motion_time);
		memcpy(this->motion_amplitude, motion_amplitude, sizeof motion_amplitude);
	}
	else{
		return poseSet;
	}

	int n = 0;
	for(int i = 0; i < numof(motions); i++) if(motion_amplitude[i] != 0. && motions[i]){
		vectime[n] = (motion_time[i]);
		vecamp[n] = (motion_amplitude[i]);
		vecmotions[n] = (motions[i]);
		n++;
	}

//	int n = vectime.size();
	if(n){
//		timemeas_t tm;
//		TimeMeasStart(&tm);
//		std::vector<MotionPose> *ret = new std::vector<MotionPose>(n);
		MotionPoseSet &set = poseSet;
		set.clear();
//		fprintf(stderr, "%lg\n", TimeMeasLap(&tm));
		for(int i = 0; i < n; i++) if(vecmotions[i]){
			MotionPose *a = new MotionPose();
			vecmotions[i]->interpolate(*a, vectime[i]);
			a->amplify(vecamp[i]);
			a->next = NULL;
			if(0 < i)
				set[i-1].next = a;
//			(*ret)[i].next = i+1 < n ? &(*ret)[i+1] : NULL;
			set.push_back(a);
		}
		return set;
//		return ret;
	}
//	else
//		return new std::vector<MotionPose>();
	return poseSet;
}

void ReZEL::motionInterpolateFree(MotionPoseSet &set){
}

void ReZEL::draw(wardraw_t *wd){
	static OpenGLState::weak_ptr<bool> init;
	double nf = nlipsFactor(*wd->vw);
	double scale = sufscale * nf;
	ReZEL *const p = this;
	if(!this->w /*|| this->docked*/)
		return;

	/* cull object */
	if(cull(*wd->vw))
		return;
	wd->lightdraws++;

	double pixels = .005 * fabs(wd->vw->gc->scale(pos)) * nf;

	draw_healthbar(this, wd, health / maxhealth(), .01 * nf, fuel / maxfuel(), -1.);

	if(!init) do{
/*		for(int i = 0 ; i < numof(models); i++){
			suf[i] = CallLoadSUF(models[i]);
			if(suf[i]){
				vbo[i] = CacheVBO(suf[i]);
				CacheSUFMaterials(suf[i]);
				suft[i] = gltestp::AllocSUFTex(suf[i]);
			}
		}*/
		model = LoadMQOModel("gundam/models/ReZEL.mqo");
		motions[0] = new Motion("gundam/models/ReZEL_waverider.mot");
		motions[1] = new Motion("gundam/models/ReZEL_airidle.mot");
		motions[2] = new Motion("gundam/models/ReZEL_aim.mot");
		motions[3] = new Motion("gundam/models/ReZEL_aimsub.mot");
		motions[4] = new Motion("gundam/models/ReZEL_airtwist.mot");
		motions[5] = new Motion("gundam/models/ReZEL_airpitch.mot");
		motions[6] = new Motion("gundam/models/ReZEL_reload.mot");
		motions[7] = new Motion("gundam/models/ReZEL_sabre.mot");
		motions[8] = new Motion("gundam/models/ReZEL_walk.mot");
		motions[9] = new Motion("gundam/models/ReZEL_stand.mot");
		motions[10] = new Motion("gundam/models/ReZEL_yaw.mot");
		motions[11] = new Motion("gundam/models/ReZEL_pitch.mot");
		motions[12] = new Motion("gundam/models/ReZEL_pitchsub.mot");
		motions[13] = new Motion("gundam/models/ReZEL_cover_right.mot");
		motions[14] = new Motion("gundam/models/ReZEL_cover_right_yaw.mot");
		motions[15] = new Motion("gundam/models/ReZEL_cover_right_pitch.mot");

		init.create(*openGLState);
	} while(0);
	
	if(true){
		glPushAttrib(GL_TEXTURE_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT | GL_ENABLE_BIT);

		glPushMatrix();
		gldTranslate3dv(this->pos);
		gldMultQuat(this->rot);
		gldScaled(scale);
		glScalef(-1, 1, -1);

		timemeas_t tm;
		TimeMeasStart(&tm);
		MotionPoseSet &v = motionInterpolate();
		motionInterpolateTime = TimeMeasLap(&tm);
		motionInterpolateTimeAverage = (motionInterpolateTimeAverage * motionInterpolateTimeAverageCount + motionInterpolateTime) / (motionInterpolateTimeAverageCount + 1);
		motionInterpolateTimeAverageCount++;
//		printf("interp[%d]: %lg, %lg\n", v.getn(), motionInterpolateTimeAverage, motionInterpolateTime);

		if(0 < muzzleFlash[0]){
			Vec3d pos;
			model->getBonePos("ReZEL_riflemuzzle", v[0], &pos);
/*			pos *= scale;
			pos[0] *= -1;
			pos[2] *= -1;
			pos = rot.trans(pos) + this->pos;*/
			glEnable(GL_LIGHT1);
			glLightfv(GL_LIGHT1, GL_POSITION, Vec4f(pos.cast<GLfloat>(), 1.));
			glLightfv(GL_LIGHT1, GL_AMBIENT, Vec4f(0,0,0,0));
			glLightfv(GL_LIGHT1, GL_DIFFUSE, Vec4f(.5f, 1.f, 1.f, 1.f));
			glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (.5 - muzzleFlash[0]) / .5 / .5);
			glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, (.5 - muzzleFlash[0]) / .5 / .05);
//			gldSpriteGlow(pos, .0015 + (.5 - muzzleFlash[0]) * .0015, Vec4<GLubyte>(127,255,255,min(muzzleFlash[0] / .3 * 255, 255)), wd->vw->irot);
		}
		else if(0 < muzzleFlash[1]){
			Vec3d pos;
			model->getBonePos("ReZEL_shieldmuzzle", v[0], &pos);
			glEnable(GL_LIGHT1);
			glLightfv(GL_LIGHT1, GL_POSITION, Vec4f(pos.cast<GLfloat>(), 1.));
			glLightfv(GL_LIGHT1, GL_AMBIENT, Vec4f(0,0,0,0));
			glLightfv(GL_LIGHT1, GL_DIFFUSE, Vec4f(1.f, .5f, 1.f, 1.f));
			glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (.3 - muzzleFlash[1]) / .5 / .05);
			glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, (.3 - muzzleFlash[1]) / .5 / .003);
//			gldSpriteGlow(pos, .0010 + (.3 - muzzleFlash[1]) * .001, Vec4<GLubyte>(255,127,255,min(muzzleFlash[1] / .3 * 255, 255)), wd->vw->irot);
		}

		DrawMQOPose(model, &v[0]);
//		YSDNM_MotionInterpolateFree(v);
		motionInterpolateFree(v);

		glPopMatrix();

		glPopAttrib();
	}
}

#define COLIST4(a) COLOR32R(a),COLOR32G(a),COLOR32B(a),COLOR32A(a)

void ReZEL::drawtra(wardraw_t *wd){
	ReZEL *p = this;
	Mat4d mat;
	double nlips = nlipsFactor(*wd->vw);
	double scale = sufscale * nlips;
	static const Quatd rotaxis(0, 1., 0., 0.);

#if PIDAIM_PROFILE
	glBegin(GL_LINES);
	glColor4ub(255,0,0,255);
	glVertex3dv(pos);
	glVertex3dv(epos);
	glColor4ub(255,255,0,255);
	glVertex3dv(pos);
	glVertex3dv(iepos);
	glEnd();
#endif

	Player *ppl = w->getPlayer();

	if(ppl && WarDraw::r_move_path && (task == Moveto || task == DeltaFormation)){
		glBegin(GL_LINES);
		glColor4ub(0,0,255,255);
		glVertex3dv(pos);
		glColor4ub(0,255,255,255);
		glVertex3dv(dest);
		glEnd();
	}

	/* cull object */
	if(cull(*wd->vw))
		return;

	if(model){
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
		Vec3d pos;
		Quatd lrot;
		MotionPoseSet &v = motionInterpolate();

		if(0 < muzzleFlash[0]){
			model->getBonePos("ReZEL_riflemuzzle", v[0], &pos, &lrot);
			pos *= scale;
			pos[0] *= -1;
			pos[2] *= -1;
			pos = rot.trans(pos) + this->pos;
			lrot = rot * rotaxis * lrot;
			gldSpriteGlow(pos, .0025 + (.5 - muzzleFlash[0]) * .0025, Vec4<GLubyte>(127,255,255,min(muzzleFlash[0] / .3 * 255, 255)), wd->vw->irot);
		}

		if(0 < muzzleFlash[1]){
			model->getBonePos("ReZEL_shieldmuzzle", v[0], &pos, &lrot);
			pos *= scale;
			pos[0] *= -1;
			pos[2] *= -1;
			pos = rot.trans(pos) + this->pos;
			lrot = rot * rotaxis * lrot;
			gldSpriteGlow(pos, .0010 + (.3 - muzzleFlash[1]) * .001, Vec4<GLubyte>(255,127,255,min(muzzleFlash[1] / .3 * 255, 255)), wd->vw->irot);
		}

		if(0 < muzzleFlash[2]){
			Vec3d gunpos[2];
			glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
			for(int i = 0; i < 2; i++){
				if(model->getBonePos(i ? "ReZEL_rvulcan" : "ReZEL_lvulcan", v[0], &gunpos[i], &lrot)){
					gunpos[i] *= sufscale;
					gunpos[i][0] *= -1;
					gunpos[i][2] *= -1;
				}
				else
					gunpos[i] = vec3_000;
				gunpos[i] = rot.trans(gunpos[i]) + this->pos;
				lrot = rot * rotaxis * lrot;

				glCallList(muzzle_texture());
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE); // Add blend
				float f = muzzleFlash[2] / .1 * 2., fi = 1. - muzzleFlash[2] / .1;
				glColor4f(f,f,f,1);
				gldTextureBeam(wd->vw->pos, gunpos[i], gunpos[i] + rot.trans(-vec3_001) * .01 * fi, .003 * fi);
//				gldSpriteGlow(gunpos[i], .0010 + (.3 - muzzleFlash[2]) * .001, Vec4<GLubyte>(255,255,127,min(muzzleFlash[2] / .3 * 255, 255)), wd->vw->irot);
			}
			glPopAttrib();
		}

		glPopAttrib();

		{
			Vec3d pos, pos0(0, 0, 40. * scale);
	/*		GLubyte col[4] = {COLIST4(cnl_shortburn[0].col)};
			pos = this->rot.trans(pos0);
			pos += this->pos;
			gldSpriteGlow(pos, p->throttle * .005, col, wd->vw->irot);
			pos0[0] = 34.5 * scale;
			pos = this->rot.trans(pos0);
			pos += this->pos;
			gldSpriteGlow(pos, p->throttle * .002, col, wd->vw->irot);
			pos0[0] = -34.5 * scale;
			pos = this->rot.trans(pos0);
			pos += this->pos;
			gldSpriteGlow(pos, p->throttle * .002, col, wd->vw->irot);*/

			GLuint texname = 0;
			const gltestp::TexCacheBind *tcb = gltestp::FindTexture("textures/blast.jpg");
			if(!tcb){
				suftexparam_t stp, stp2;
				stp.flags = STP_MAGFIL;
				stp.magfil = GL_LINEAR;
				stp2.flags = STP_ENV | STP_MAGFIL;
				stp2.env = GL_MODULATE;
				stp2.magfil = GL_LINEAR;
	//			texname = CallCacheBitmap("textures/blast.jpg", "textures/blast.jpg", &stp, NULL);
				texname = CallCacheBitmap5("textures/blast.jpg", "textures/blast.jpg", &stp, "textures/noise.jpg", &stp2);
			}
			else
				texname = tcb->getList();

			double tim = -30. * wd->w->war_time();
			RandomSequence rs((unsigned long)this);

			glPushMatrix();
			glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
			glCallList(texname);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Add blend
			float amp = MIN(float(fabs(this->throttle)) * 30.f, 1.);

			MultiTextureInit();
			glActiveTextureARB(GL_TEXTURE1_ARB);
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glRotated(147.43462, 0, 0, 1);
			glScaled(.2, .2, .2);
			glMatrixMode(GL_MODELVIEW);
			glColor4f(1., 1., 1., amp);

			// Thruster ejection effects
			Vec3d thrustVector = velo.slen() < .1 * .1 ? velo / .1 : velo.norm();
			for(int i = 0; i < numof(thrusterDir); i++){
				Vec3d pos0;
				if(model->getBonePos(gltestp::dstring("ReZEL_thruster") << i, v[0], &pos0, &lrot)){
					Vec3d dir = (rot * rotaxis * lrot).trans(thrusterDirs[i]);
					double mag = thrusterPower[i]/*-thrustVector.sp(dir)*/;
					if(0. < mag){
						pos0 *= sufscale;
						pos0[0] *= -1;
						pos0[2] *= -1;
						Vec3d pos = rot.trans(pos0) + this->pos;
//						dir = (dir).norm();
						gldScrollTextureBeam(wd->vw->pos, pos, pos + dir * mag * .01, mag * .0025, tim + 100. * rs.nextd());
					}
				}
			}

/*			if(0. < throttle){
				gldScrollTextureBeam(wd->vw->pos, this->pos + this->rot.trans(Vec3d(0,0,30.*scale)), this->pos + this->rot.trans(Vec3d(0,0,30.*scale+.010*amp)), .0025*amp, tim + 100. * rs.nextd());
				gldScrollTextureBeam(wd->vw->pos, this->pos + this->rot.trans(Vec3d( 34.5*scale,0,40.*scale)), this->pos + this->rot.trans(Vec3d( 34.5*scale,0,40.*scale+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
				gldScrollTextureBeam(wd->vw->pos, this->pos + this->rot.trans(Vec3d(-34.5*scale,0,40.*scale)), this->pos + this->rot.trans(Vec3d(-34.5*scale,0,40.*scale+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
			}
			else{
				double ofs = 5.*scale;
				pos = this->pos + this->rot.trans(Vec3d(0,0,35.*scale));
				Quatd upangle = this->rot * Quatd(sin(M_PI*5./6./2.),0.,0.,cos(M_PI*5./6./2.));
				Quatd dnangle = this->rot * Quatd(sin(-M_PI*5./6./2.),0.,0.,cos(-M_PI*5./6./2.));
				gldScrollTextureBeam(wd->vw->pos, pos + upangle.trans(Vec3d( 34.5*scale,0,ofs)), pos + upangle.trans(Vec3d( 34.5*scale,0,ofs+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
				gldScrollTextureBeam(wd->vw->pos, pos + dnangle.trans(Vec3d( 34.5*scale,0,ofs)), pos + dnangle.trans(Vec3d( 34.5*scale,0,ofs+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
				gldScrollTextureBeam(wd->vw->pos, pos + upangle.trans(Vec3d(-34.5*scale,0,ofs)), pos + upangle.trans(Vec3d(-34.5*scale,0,ofs+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
				gldScrollTextureBeam(wd->vw->pos, pos + dnangle.trans(Vec3d(-34.5*scale,0,ofs)), pos + dnangle.trans(Vec3d(-34.5*scale,0,ofs+.005*amp)), .00125*amp, tim + 100. * rs.nextd());
			}*/
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glActiveTextureARB(GL_TEXTURE0_ARB);
			glPopAttrib();
			glPopMatrix();
		}
//		YSDNM_MotionInterpolateFree(v);
//		delete pv;
		motionInterpolateFree(v);
	}

	// Beam sabre
	if(0. < fsabre && model){
		MotionPoseSet &v = motionInterpolate();

		struct gldBeamsData bd;
		Vec3d v0(.0, .0, .0);
		const Vec3d &viewpos = wd->vw->pos;
		int lumi;
		struct random_sequence rs;
		double viewdist;
		const double widscale = 10.;
		bd.cc = bd.solid = 0;
		init_rseq(&rs, (long)(wd->vw->viewtime * 1e6) + (unsigned long)this);
		lumi = 191 + rseq(&rs) % 63;
		Vec3d pos;
		Quatd lrot;
		model->getBonePos("ReZEL_riflemuzzle", v[0], &pos, &lrot);
		pos *= scale;
		pos[0] *= -1;
		pos[2] *= -1;
		pos = rot.trans(pos) + this->pos;
		lrot = rot * rotaxis * lrot;
		mat = lrot.tomat4();
		mat.vec3(3) = pos;
		Vec3d end = mat.vp3(v0);

		viewdist = (end - viewpos).len();
//		drawglow(end, wd->irot, .0015, COLOR32RGBA(255,255,255, 127 / (1. + viewdist)));
		glColor4ub(255,255,255,255);
		Vec4<GLubyte> col(255,255,255, 127 / (1. + viewdist));
		gldTextureGlow(end, .0020, col, wd->vw->irot);

		gldBeams(&bd, viewpos, end, .00001 * widscale, COLOR32RGBA(3,127,191,0));
		v0[1] -= .0003 * widscale;
		end = mat.vp3(v0);
		gldBeams(&bd, viewpos, end, .00003 * widscale, COLOR32RGBA(255,127,255,lumi));
		v0[1] -= .0003 * widscale;
		end = mat.vp3(v0);
		gldBeams(&bd, viewpos, end, .00003 * widscale, COLOR32RGBA(255,131,255,lumi));
		v0[1] -= .0003 * widscale;
		end = mat.vp3(v0);
		gldBeams(&bd, viewpos, end, .00001 * widscale, COLOR32RGBA(255,191,255,0));

//		YSDNM_MotionInterpolateFree(v);
//		delete pv;
		motionInterpolateFree(v);
	}

#if 0 /* thrusters appear unimpressing */
	tankrot(mat, pt);
	{
		avec3_t v;
		int j;
		VECADD(v, pt->omg, pt->pos);
		glColor4ub(255,127,0,255);
		glBegin(GL_LINES);
		glVertex3dv(pt->pos);
		glVertex3dv(v);
		glEnd();
		VECADD(v, p->aac, pt->pos);
		glColor4ub(255,127,255,255);
		glBegin(GL_LINES);
		glVertex3dv(pt->pos);
		glVertex3dv(v);
		glEnd();

		v[0] = p->thrusts[0][0];
		v[1] = p->thrusts[0][1];
		for(j = 0; j < 2; j++) if(v[j]){
			avec3_t v00[2] = {{0., 0., .003}, {0., 0., -.003}};
			int i;
			for(i = 0; i < 2; i++){
				struct gldBeamsData bd;
				avec3_t end;
				int lumi;
				struct random_sequence rs;
				int sign = (i + j) % 2 * 2 - 1;
				avec3_t v0;
				VECCPY(v0, v00[i]);
				bd.cc = bd.solid = 0;
				init_rseq(&rs, (long)(wd->gametime * 1e6) + i + pt);
				lumi = rseq(&rs) % 256 * min(fabs(v[j]) / .2, 1.);
				v0[1] += sign * .001;
				mat4vp3(end, mat, v0);
				gldBeams(&bd, wd->view, end, .00001, COLOR32RGBA(255,223,128,0));
				v0[1] += sign * .0003;
				mat4vp3(end, mat, v0);
				gldBeams(&bd, wd->view, end, .00007, COLOR32RGBA(255,255,255,lumi));
				v0[1] += sign * .0003;
				mat4vp3(end, mat, v0);
				gldBeams(&bd, wd->view, end, .00009, COLOR32RGBA(255,255,223,lumi));
				v0[1] += sign * .0003;
				mat4vp3(end, mat, v0);
				gldBeams(&bd, wd->view, end, .00005, COLOR32RGBA(255,255,191,0));
			}
		}
	}
#endif

#if 0
	if(false && mf) for(int i = 0; i < 2; i++){
		Vec3d pos = rot.trans(Vec3d(gunPos[i])) * nlips + this->pos;
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
		glCallList(muzzle_texture());
/*		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glRotatef(-90, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);*/
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE); // Add blend
		float f = mf / .1 * 2., fi = 1. - mf / .1;
		glColor4f(f,f,f,1);
		gldTextureBeam(wd->vw->pos, pos, pos + rot.trans(-vec3_001) * .03 * fi, .01 * fi);
/*		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);*/
		glPopAttrib();
	}
#endif

	// Find and display cover points
	if(w->getPlayer()->chase == this){
		GetCoverPointsMessage com;
		com.org = pos;
		com.radius = .3;
		if(w->sendMessage(com)){
			CoverPointVector &vcp = com.cpv;
			for(int i = 0; i < vcp.size(); i++){
				const CoverPoint &cp = vcp[i];
				glPushMatrix();
				gldTranslate3dv(cp.pos);
				gldMultQuat(cp.rot);
				glColor4f(1,.5,.5,1.);

				glBegin(GL_LINES);
				glVertex3d(0, -.1, 0.);
				glVertex3d(0, .1, 0.);
				glVertex3d(-.1, 0, 0.);
				glVertex3d(.1, 0, 0.);
				glVertex3d(0, 0, -.1);
				glVertex3d(0, 0, .1);
				glEnd();

				if(cp.type == cp.LeftEdge)
					glScaled(-1, 1, 1);

				glBegin(GL_LINE_LOOP);
				glVertex3d(-.05, .06, 0.05);
				glVertex3d( .02, .06, 0.05);
				glVertex3d( .02, .07, 0.05);
				glVertex3d( .05, .05, 0.05);
				glVertex3d( .02, .03, 0.05);
				glVertex3d( .02, .04, 0.05);
				glVertex3d(-.05, .04, 0.05);
				glEnd();

				glPopMatrix();
			}
		}
	}
}

void ReZEL::drawHUD(WarDraw *wd){
	if(game->player->mover != game->player->cockpitview)
		return;
	GLpmatrix pm;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	int wmin = min(wd->vw->vp.w, wd->vw->vp.h);
	double xf = (double)wd->vw->vp.w / wmin;
	double yf = (double)wd->vw->vp.h / wmin;
	glOrtho(-xf, xf, -yf, yf, -1, 1);
//	glOrtho(0, wd->vw->vp.w, wd->vw->vp.h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// Crosshair
	glColor4f(1,1,1,1);
	glBegin(GL_LINES);
	glVertex2d(-.05, 0);
	glVertex2d( .05, 0);
	glVertex2d(0, -.05);
	glVertex2d(0,  .05);
	glEnd();

	glScaled(.2, .2, .2);
	glColor4f(1,1,1,.3);

	glBegin(GL_QUAD_STRIP);
	glVertex2d(-.3, .3);
	glVertex2d(-.4, .3);
	glVertex2d(-.4, .15);
	glVertex2d(-.5, .15);
	glVertex2d(-.4, -.15);
	glVertex2d(-.5, -.15);
	glVertex2d(-.3, -.3);
	glVertex2d(-.4, -.3);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glVertex2d( .3, .3);
	glVertex2d( .4, .3);
	glVertex2d( .4, .15);
	glVertex2d( .5, .15);
	glVertex2d( .4, -.15);
	glVertex2d( .5, -.15);
	glVertex2d( .3, -.3);
	glVertex2d( .4, -.3);
	glEnd();

	// Ammo indicator
	for(int n = 0; n < 2; n++){
		int divisor = n ? rifleMagazineSize : vulcanMagazineSize;
		glPushMatrix();
		glTranslated((n * 2 - 1) * .2, 0., 0.);
		for(int m = 0; m < 2; m++){
			for(int i = 0; i < divisor; i++){
				double s0 = sin((n * 2 - 1) * (i + .1) * M_PI / divisor);
				double c0 = -cos((i + .1) * M_PI / divisor);
				double s1 = sin((n * 2 - 1) * (i + .9) * M_PI / divisor);
				double c1 = -cos((i + .9) * M_PI / divisor);
				double r0 = .8;
				double r1 = 1.;
				if(m){
					float f = (n ? freload != 0. : vulcanCooldownTime < vulcancooldown) ?
						i < divisor * (n ? 1. - freload / reloadTime : 1. - vulcancooldown / vulcanReloadTime) ? 1. : 0. :
						(n ? i < magazine : i < vulcanmag) ? 1. : 0.;
					glColor4f(1, f, f, 1);
				}
				else{
					if(n ? freload != 0. : vulcanCooldownTime < vulcancooldown)
						glColor4f(1,0,0, i < divisor * (n ? 1. - freload / reloadTime : 1. - vulcancooldown / vulcanReloadTime) ? .8 : .3);
					else
						glColor4f(1,1,1, (n ? i < magazine : i < vulcanmag) ? .8 : .3);
				}
				glBegin(m ? GL_LINE_LOOP : GL_QUADS);
				glVertex2d(r0 * s0, r0 * c0);
				glVertex2d(r1 * s0, r1 * c0);
				glVertex2d(r1 * s1, r1 * c1);
				glVertex2d(r0 * s1, r0 * c1);
				glEnd();
			}
		}
		glPopMatrix();
	}

	glPopMatrix();
}

void ReZEL::drawOverlay(wardraw_t *){
	glScaled(10, 10, 1);
	glBegin(GL_LINE_LOOP);
	glVertex2d(-.10, -.10);
	glVertex2d(-.05,  .00);
	glVertex2d(-.10,  .10);
	glVertex2d( .00,  .05);
	glVertex2d( .10,  .10);
	glVertex2d( .05,  .00);
	glVertex2d( .10, -.10);
	glVertex2d( .00, -.05);
	glEnd();
}










class ReZELchartCmdRegister{
	class ReZELDrawTimeChartSeries : public GLWchart::TimeChartSeries{
	public:
		ReZELDrawTimeChartSeries() : TimeChartSeries(-1, Vec4f(1,0,0,1)){}
		virtual double timeProc(double dt){
			return ReZEL::motionInterpolateTime;
		}
	};
	class ReZELInterpAvgTimeChartSeries : public GLWchart::TimeChartSeries{
	public:
		ReZELInterpAvgTimeChartSeries() : TimeChartSeries(-1, Vec4f(0,1,0,1)){}
		virtual double timeProc(double dt){
			return ReZEL::motionInterpolateTimeAverage;
		}
	};
	class FrameTimeChartSeries : public GLWchart::TimeChartSeries{
	public:
		FrameTimeChartSeries() : TimeChartSeries(-1, Vec4f(0,0,1,1)){}
		virtual Vec4f color()const{return Vec4f(0.5,0.5,1,1);}
		virtual double timeProc(double dt){return dt;}
	};
	class SqTimeChartSeries : public GLWchart::TimeChartSeries{
	public:
		SqTimeChartSeries() : TimeChartSeries(-1, Vec4f(1,0,1,1)){}
		virtual Vec4f color()const{return Vec4f(1,0.5,.5,.5);}
		virtual double timeProc(double dt){
			HSQUIRRELVM v = ReZEL::sqvm;
			sqa::StackReserver sr(v);
			sq_pushroottable(v);
			sq_pushstring(v, _SC("sqtimechart"), -1);
			if(SQ_SUCCEEDED(sq_get(v, -2))){
				sq_pushroottable(v);
				if(SQ_FAILED(sq_call(v, 1, SQTrue, SQTrue)))
					return 0.;
				SQFloat f;
				if(SQ_SUCCEEDED(sq_getfloat(v, -1, &f)))
					return f;
				else
					return 0.;
			}
			else
				return 0;
		}
	};

/*	static int cmd_chart(int argc, char *argv[]){
		GLWchart *wnd = new GLWchart(game "MotionInterpolateTime", new ReZELDrawTimeChartSeries);
		wnd->addSeries(new ReZELInterpAvgTimeChartSeries());
		wnd->addSeries(new FrameTimeChartSeries());
		wnd->addSeries(new SqTimeChartSeries());
		glwAppend(wnd);
		return 0;
	}*/
public:
	ReZELchartCmdRegister(){
//		CmdAdd("ReZELchart", cmd_chart);
	}
} ReZELchartcmdreg;


