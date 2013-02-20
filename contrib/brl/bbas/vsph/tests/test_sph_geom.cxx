#include <testlib/testlib_test.h>

#include <vsph/vsph_view_sphere.h>
#include <vsph/vsph_sph_point_2d.h>
#include <vsph/vsph_sph_box_2d.h>
#include <vsph/vsph_utils.h>
#include <vcl_fstream.h>
#include <vnl/vnl_math.h>

static vcl_string MyDIR = "C:/Users/mundy/VisionSystems/Finder/VolumetricQuery/";

static void test_sph_geom()
{
  double er = 0;
  //test azimuth difference
  double az_a = 15.0, az_b = 180.0;
  double ang1=0, ang2=0;
  double diff0 = vsph_utils::azimuth_diff(az_a, az_b, false);
  vsph_utils::half_angle(az_a, az_b, ang1, ang2, false);
  double haer0 =  vcl_fabs(ang1-97.5)+vcl_fabs(ang2+82.5);
  az_a = -15.0, az_b = -180.0;
  double diff1 = vsph_utils::azimuth_diff(az_a, az_b, false);
  vsph_utils::half_angle(az_a, az_b, ang1, ang2, false);
  double haer1 =  vcl_fabs(ang1+97.5)+vcl_fabs(ang2-82.5);
  az_a = -90.0, az_b = 91.0;
  double diff2 = vsph_utils::azimuth_diff(az_a, az_b, false);
  vsph_utils::half_angle(az_a, az_b, ang1, ang2, false);
   double haer2 =  vcl_fabs(ang1+179.5)+vcl_fabs(ang2-0.5);
  az_a = -179.0, az_b = 180.0;
  double diff3 = vsph_utils::azimuth_diff(az_a, az_b, false);
  vsph_utils::half_angle(az_a, az_b, ang1, ang2, false);
  double haer3 =  vcl_fabs(ang1+179.5)+vcl_fabs(ang2-0.5);
  az_a = 179.0, az_b = -180.0;
  double diff4 = vsph_utils::azimuth_diff(az_a, az_b, false);
  vsph_utils::half_angle(az_a, az_b, ang1, ang2, false);
  double haer4 =  vcl_fabs(ang1-179.5)+vcl_fabs(ang2+0.5);
  er = vcl_fabs(diff0-165.0) + vcl_fabs(diff1 + 165.0);
  er += vcl_fabs(diff2+179.0) + vcl_fabs(diff3+1.0);
  er += vcl_fabs(diff4-1.0);
  TEST_NEAR("angle difference ", er, 0.0, 0.01);
  er = haer0 + haer1 + haer2 + haer3 + haer4;
  TEST_NEAR("half angle ", er, 0.0, 0.01);

  // test box spanning 360 degrees in azimuth
  vsph_sph_point_2d p10(80.0, 0.0, false);
  vsph_sph_point_2d p11(110.0,10.0, false);
  vsph_sph_point_2d p12(80.0, 60.0, false);
  vsph_sph_point_2d p13(110.0, 110.0, false);
  vsph_sph_point_2d p14(80.0, 165.0, false);
  vsph_sph_point_2d p14a(80.0, 166.0, false);
  vsph_sph_point_2d p15(110.0, -175.0, false);
  vsph_sph_point_2d p15a(110.0, -180.0, false);
  vsph_sph_point_2d p16(80.0, -130.0, false);
  vsph_sph_point_2d p17(110.0, -100.0, false);
  vsph_sph_point_2d p18(80.0,  45.0, false);
  vsph_sph_point_2d p19(80.0, -10.0, false);
  vsph_sph_point_2d p20(80.0, 179.0, false);
  vsph_sph_box_2d bb_long(p10, p14, p15);
  bb_long.print(vcl_cout, false);
  bool c16 = bb_long.contains(p16);//true
  bool c11 = bb_long.contains(p11);//false
  bb_long.add(p11);
  bool c11add = bb_long.contains(p11);//true
  bool c12 = bb_long.contains(p12);//false
  bool c13 = bb_long.contains(p13);//false
  bool c14a = bb_long.contains(p14a);//true
  bool c15a = bb_long.contains(p15a);//true
  bool test1 = c16&&!c11&&c11add&&!c12&&!c13&&c14a&&c15a;
  TEST("Long interval contains c", test1, true);
  vsph_sph_box_2d bb_short(p10, p12, p11);
  bool c12s = bb_short.contains(p12);//true
  bool c15s = bb_short.contains(p15);//false
  bool c18s = bb_short.contains(p18);//true
  bool test2 = c12s&&!c15s&&c18s;
  TEST("Short interval contains c", test2, true);

  vsph_sph_box_2d bb_ext(p10, p12, p11);
  bb_ext.add(p14); 
  bb_ext.add(p15a);
  TEST("bb_ext.contains(p15)", bb_ext.contains(p15), false);
  TEST("bb_ext.contains(p20)", bb_ext.contains(p20), true);
  bb_ext.add(p15);
  TEST("bb_ext.contains(p15)", bb_ext.contains(p15), true);
  // test incremental updates 
  vsph_sph_box_2d bb_inc(false);
  bb_inc.add(p10); bb_inc.add(p11); bb_inc.add(p12);
  bool inc18 = bb_inc.contains(p18);//true
  bool incp15a = bb_inc.contains(p15a);//false
  vsph_sph_box_2d bb_inc_cut(false);
  bb_inc_cut.add(p14a);bb_inc_cut.add(p15);bb_inc_cut.add(p15a);
  bool inc_cut_14 = bb_inc_cut.contains(p14);//false
  bool inc_cut_20 = bb_inc_cut.contains(p20);//true
  bool test_inc = inc18&&!incp15a&&!inc_cut_14&&inc_cut_20;
  TEST("incremental update", test_inc, true);
  // test constructors
  // points in degrees
  vsph_sph_point_2d p0(0.0, 0.0, false), p1(90.0, 90.0, false), pc(45.0, 45.0, false);
  // box in radians
  vsph_sph_box_2d bb(p0, p1, pc);
  double mnth = bb.min_theta(false), mxth = bb.max_theta(false);
  double mnph = bb.min_phi(false), mxph = bb.max_phi(false);
  er = mnph + mnth + vcl_fabs(mxth-90.0)+ vcl_fabs(mxph-90.0);
  er += vcl_abs(bb.min_theta()+ bb.min_phi());
  TEST_NEAR("constructors", er, 0.0, 0.01);
  // test box area
  double area = bb.area();
  TEST_NEAR("Sperical Area", area, vnl_math::pi_over_2, 0.01);

  // test box contains
  vsph_sph_box_2d bin(p10, p12, p11), bout(p19, p13, p11);
  bool box_inside = bout.contains(bin);
  TEST("box contains box", box_inside, true);
  // test boxes spanning cut
  vsph_sph_point_2d p2(70.0, 180.0, false), p3(90.0, -150.0, false),p3c(90.0, -170.0, false);
  vsph_sph_point_2d p4(80.0, 170.0, false), p5(100.0, -170.0, false), p5c(100.0, 180.0, false);
  vsph_sph_box_2d bba(p2, p3, p3c), bbb(p4,p5,p5c);
  double min_ph_a = bba.min_phi(false), max_ph_a = bba.max_phi(false);
  double min_ph_b = bbb.min_phi(false), max_ph_b = bbb.max_phi(false);
  bool min_altb = vsph_utils::a_lt_b(min_ph_a, min_ph_b, false);
  bool max_blta = vsph_utils::a_lt_b(max_ph_b, max_ph_a, false);
  TEST("comparison operators", (!min_altb)&&max_blta, true);

  double area_a = bba.area();
  double area_b = bbb.area();
  er = vcl_fabs(area_a-0.179081) + vcl_fabs(area_b-0.121229);
  TEST_NEAR("areas spanning +-180", er, 0.0, 0.001);
  //test intersection of boxes spanning cut
  vcl_vector<vsph_sph_box_2d> boxes;
  bool good = intersection(bba, bbb, boxes);
  vsph_sph_box_2d bint;
  if (good) 
   bint = boxes[0];
  double min_ph_int = bint.min_phi(false), max_ph_int = bint.max_phi(false);
  double min_th_int = bint.min_theta(false), max_th_int = bint.max_theta(false);

  good = good && vsph_utils::a_eq_b(min_ph_int, p2.phi_, false)
              && vsph_utils::a_eq_b(max_ph_int, p5.phi_, false)
              && vsph_utils::a_eq_b(min_th_int, p4.theta_, false)
              && vsph_utils::a_eq_b(max_th_int, p3.theta_,false);
  TEST("intersection", good, true);
  // test boxes forming a cross (no endpoints inside each box
  vsph_sph_point_2d p6 ( 50.0,  -10.0, false);
  vsph_sph_point_2d p7 (130.0,   10.0, false);
  vsph_sph_point_2d p8 ( 90.0,    0.0, false); 
  vsph_sph_point_2d p9 ( 75.0,  -60.0, false); 
  vsph_sph_point_2d p100(105.0,   60.0, false);

  vsph_sph_box_2d bbc1(p6, p7, p8), bbc2(p9,p100,p8);
  boxes.clear();
  good = intersection(bbc1, bbc2, boxes);
  vsph_sph_box_2d bint_cross;
  if (good) bint_cross = boxes[0];
  boxes.clear();
  good = good && intersection(bbc2, bbc1, boxes);
  vsph_sph_box_2d bint_cross_rev;
  if (good) bint_cross_rev = boxes[0];
  good = vsph_utils::a_eq_b(bint_cross.min_phi(false), p6.phi_, false)
      && vsph_utils::a_eq_b(bint_cross.max_phi(false), p7.phi_, false)
      && vsph_utils::a_eq_b(bint_cross_rev.max_phi(false),bint_cross.max_phi(false) , false)
      && bint_cross.min_theta(false) == p9.theta_
      && bint_cross.max_theta(false) == p100.theta_;
  TEST("Crossing box arrangement", good, true);
  // one box essentially covers the entire +- 180 phi circle
  vsph_sph_box_2d box_s1, box_s2, box_s12;
  double b1_thmin = 1.4836689630573823, b1_thmax = 1.6579236905324111;
  double b1_phia = -vnl_math::pi_over_2, b1_phib =vnl_math::pi_over_2;
  double b1_phic = 0.51656139130052758;
  box_s1.set(b1_thmin, b1_thmax, b1_phia, b1_phib, b1_phic);
  double b2_thmin = 1.3088871075562318, b2_thmax = vnl_math::pi*7/12;
  double b2_phia =-0.31728356503269012, b2_phib = -0.31911878808324995;
  double b2_phic = 0.54737987781481912;
  box_s2.set(b2_thmin, b2_thmax, b2_phia, b2_phib, b2_phic);
  boxes.clear();
  // two boxes are produced
  good = intersection(box_s1, box_s2, boxes)
      && box_s1.contains(boxes[0])&& box_s1.contains(boxes[1]);
  TEST("each box contains the other's bounds", good, true);
  double tol = 0.001;
  vcl_string box_path = MyDIR + "box_display.wrl";
#if 0
  vcl_vector<vgl_vector_3d<double> > verts;
  vcl_vector<vcl_vector<int> > quads;
  box_s1.planar_quads(verts, quads, tol);
  vcl_ofstream os(box_path.c_str());
  box_s1.display_box(os, 1.0f, 0.0f, 0.0f, tol);
  os.close();
#endif
#if 0
  vcl_vector<vsph_sph_box_2d> dis_boxes(2);
  dis_boxes[0]=box_s1;   dis_boxes[1]=box_s2;
  vcl_vector<float> c0(3, 0.0f), c1(3, 0.0f);
  c0[0]=1.0f; c1[0]=1.0f; c1[1] = 1.0f;
  vcl_vector<vcl_vector<float> > colors(2);
  colors[0]=c0; colors[1]=c1;
  vsph_sph_box_2d::display_boxes(box_path, dis_boxes, colors, tol);
#endif
  double grok_a_phi = -2.7617465101715433;
  double grok_b_phi = -2.4723473031673180;
  double grok_c_phi = -2.5151716844356731;
  double grok_min_th  = 1.6235700411813854;
  double grok_max_th = 1.8268944131886669;
  vsph_sph_box_2d grok;
  grok.set(grok_min_th, grok_max_th, grok_a_phi, grok_b_phi, grok_c_phi, true);
  vcl_string grok_path = MyDIR + "grok_box_display.wrl";
  vcl_ofstream os(grok_path.c_str());
  grok.display_box(os, 1.0f, 1.0f, 0.0f, tol);
  os.close();
}

TESTMAIN(test_sph_geom);