//:
// \file
// \brief executable to match a given volumetric query and a camera estimate to an indexed reference volume
// \author Ozge C. Ozcanli
// \date Oct 8, 2012

#include <volm/volm_io.h>
#include <volm/volm_tile.h>
#include <vul/vul_arg.h>
#include <vil/vil_image_view.h>
#include <vil/vil_save.h>
#include <volm/volm_spherical_container.h>
#include <volm/volm_spherical_container_sptr.h>
#include <volm/volm_spherical_shell_container.h>
#include <volm/volm_spherical_shell_container_sptr.h>
#include <volm/volm_io.h>
#include <volm/volm_query.h>
#include <volm/volm_query_sptr.h>
#include <volm/volm_loc_hyp.h>
#include <boxm2/volm/boxm2_volm_wr3db_index.h>
#include <boxm2/volm/boxm2_volm_wr3db_index_sptr.h>
#include <boxm2/volm/boxm2_volm_matcher.h>
#include <vpgl/vpgl_perspective_camera.h>
#include <bbas/bocl/bocl_manager.h>
#include <bbas/bocl/bocl_device.h>
#include <vcl_set.h>
#include <vul/vul_timer.h>

int main(int argc,  char** argv)
{
  vul_arg<vcl_string> cam_file("-cam", "cam kml filename", "");
  vul_arg<vcl_string> label_file("-label", "xml file with labeled polygons", "");
  vul_arg<vcl_string> ind_loc("-indloc", "location for binary file hypothesis", "");
  vul_arg<vcl_string> ind_pre("-ind", "prefix of binary file", "");
  vul_arg<vcl_string> hypo_file("-hypo", "hypothese file for current indices","");
  vul_arg<unsigned> start("-start", "start value of indices in hypothese",0);
  vul_arg<unsigned> skip("-skip", "skip value, how many hypos will be skipped",1);
  vul_arg<float> buffer_capacity("-buff", "index buffer capacity (GB)",10.0f);
  vul_arg<float> vmin("-v", "minimum voxel size (m)", 10.0f);
  vul_arg<float> dmax("-d", "maximum depth", 60000.0f);
  vul_arg<float> solid_angle("-sa", "solid angle (deg)", 2.0f);
  vul_arg<float> cap_angle("-ca", "cap angle(deg)", 360.0);
  vul_arg<float> point_angle("-pa", "point angle(deg)", 10.0);
  vul_arg<float> top_angle("-top", "top angle(deg)" , 0.0f);
  vul_arg<float> bottom_angle("-btm", "bottom angle(deg)", 0.0f);
  vul_arg<vcl_string> out_folder("-out", "output folder", "");
  vul_arg<int> save_images("-save", "save out images or not", 0);
  vul_arg<vcl_string> dev1("-gpuid0", "the idx for the 1st GPU", "gpu0");
  vul_arg<vcl_string> dev2("-gpuid1", "the idx for the 2nd GPU", "");
  vul_arg<vcl_string> dev3("-gpuid2", "the idx for the 3rd GPU", "");
  vul_arg<vcl_string> dev4("-gpuid3", "thd idx for the 4th GPU", "");
  vul_arg_parse(argc, argv);

  vcl_cout << "argc: " << argc << vcl_endl;
  //: check input parameters
  if (cam_file().compare("") == 0 || label_file().compare("") == 0 ||
	  ind_loc().compare("") == 0 || ind_pre().compare("") == 0 || hypo_file().compare("") == 0 ||
	  out_folder().compare("") == 0) {
    vcl_cerr << "EXE_ARGUMENT_ERROR!\n";
    volm_io::write_status(out_folder(), volm_io::EXE_ARGUMENT_ERROR);
    vul_arg_display_usage_and_exit();
    return volm_io::EXE_ARGUMENT_ERROR;
  }

  volm_io::write_status(out_folder(), volm_io::EXE_STARTED);

  vul_timer t;

  //: define the devices that will be used
  //: make bocl manager

  bocl_manager_child_sptr mgr = bocl_manager_child::instance();
  vcl_vector<bocl_device_sptr> gpus;
  unsigned numGPU = 0;
  vcl_set<unsigned> gpuIdx;
  if(dev1().compare("") != 0)  { 
    numGPU++; 
	vcl_string str = dev1().substr(3,dev1().size());
	unsigned idx; 
	vcl_istringstream(str) >> idx;
	gpuIdx.insert(idx);
  }
  if(dev2().compare("") != 0) { 
    numGPU++; 
	vcl_string str = dev2().substr(3,dev2().size());
	unsigned idx; 
	vcl_istringstream(str) >> idx;
	gpuIdx.insert(idx);
  }
  if(dev3().compare("") != 0) {
    numGPU++;
	vcl_string str = dev3().substr(3,dev3().size());
	unsigned idx; 
	vcl_istringstream(str) >> idx;
	gpuIdx.insert(idx);
  }
  if(dev4().compare("") != 0) {
    numGPU++;
	vcl_string str = dev4().substr(3,dev4().size());
	unsigned idx; 
	vcl_istringstream(str) >> idx;
	gpuIdx.insert(idx);
  }

  if((unsigned)mgr->numGPUs() < numGPU){
    vcl_cerr << " GPU desired to use " << numGPU << " is too big, only " << mgr->gpus_.size() <<" available, reduce the number of gpu usage\n";
    volm_io::write_status(out_folder(), volm_io::EXE_ARGUMENT_ERROR);
    vul_arg_display_usage_and_exit();
    return volm_io::EXE_ARGUMENT_ERROR;
  }
  vcl_set<unsigned>::iterator sit = gpuIdx.begin();
  for(; sit != gpuIdx.end(); ++sit) {
    unsigned idx = *sit;
	if(idx < mgr->gpus_.size())  gpus.push_back(mgr->gpus_[idx]);
  }
  
  
    //: check the query input file
  depth_map_scene_sptr dm = new depth_map_scene;
  vcl_string img_category;
  if (!volm_io::read_labelme(label_file(), dm, img_category)) {
    volm_io::write_status(out_folder(), volm_io::LABELME_FILE_IO_ERROR);
  }

  //: check the camera input file
  double heading, heading_dev, tilt, tilt_dev, roll, roll_dev;
  double top_fov, top_fov_dev;
  if (!volm_io::read_camera(cam_file(), dm->ni(), dm->nj(), heading, heading_dev, tilt, tilt_dev, roll, roll_dev, top_fov, top_fov_dev)) {
    volm_io::write_status(out_folder(), volm_io::CAM_FILE_IO_ERROR);  
  }

  //: create containers
  
  volm_spherical_container_sptr sph = new volm_spherical_container(solid_angle(),vmin(),dmax());
  volm_spherical_shell_container_sptr sph_shell = new volm_spherical_shell_container(1.0, cap_angle(), point_angle(), top_angle(), bottom_angle());

  //: create query array
  volm_query_sptr query = new volm_query(cam_file(), label_file(), sph, sph_shell);

  //: screen output
  // query check
  dm = query->depth_scene();
  vcl_cout << " The " << dm->ni() << " x " << dm->nj() << " query image has following defined depth region " << vcl_endl;
  if(dm->sky()){
    vcl_cout << "\t sky region, min_depth = " << 255 << vcl_endl;
  }
  if(dm->scene_regions().size()) {
    for(unsigned i = 0; i < dm->scene_regions().size(); i++) {
	  vcl_cout << "\t " <<  (dm->scene_regions())[i]->name()  << " region "  
		       << ",\t min_depth = " << (dm->scene_regions())[i]->min_depth() 
			   << ",\t min_depth_interval = " << (int)sph->get_depth_interval((dm->scene_regions())[i]->min_depth())
			   << ",\t max_depth = " << (dm->scene_regions())[i]->max_depth() 
			   << ",\t order = " << (dm->scene_regions())[i]->order()
			   << vcl_endl;
	}
  }
  vcl_cout << " for spherical surface, point angle = " << point_angle() << " degree, " 
	       << ", top_angle = " << top_angle()
		   << ", bottom_angle = " << bottom_angle()
	       << ", generated query has size " << query->get_query_size() << vcl_endl;

  vcl_cout << " The query has " << query->get_cam_num() << " cameras: " << vcl_endl;
  vcl_cout << " \t" << query->headings().size() << " headings = " << query->headings()[0] << " +/- " << (query->headings().size()-1)/2 << vcl_endl;
  vcl_cout << " \t" << query->tilts().size() << " tilts = " << query->tilts()[0] << " +/- " << (query->tilts().size() - 1)/2 << vcl_endl;
  vcl_cout << " \t" << query->rolls().size() << " rolls = " << query->rolls()[0] << " +/- " << (query->rolls().size() - 1)/2 << vcl_endl;
  vcl_cout << " \t" << query->top_fovs().size() << " top_fov = " << query->top_fovs()[0] << " +/- " << (query->top_fovs().size() - 1)/2 << vcl_endl;
  vcl_cout << " Generated query_size for 1 camera is " << query->get_query_size() << " byte, "
	       << " gives total query size = " << query->get_cam_num() << " x " << query->get_query_size() 
		   << " = " << (double)query->get_cam_num()*(double)query->get_query_size()/(1024*1024*1024) << " GB"
	       << vcl_endl;
  vcl_cout << " query is created in " << t.all()/1000 << " seconds " << vcl_endl;
 
  //: load the hypothesis and create indices
  vcl_string ind_txt = ind_loc() + ind_pre() + ".txt";
  vcl_string ind_file = ind_loc() + ind_pre() + ".bin";
  vcl_ifstream ind_txt_file(ind_txt.c_str(), vcl_ios::in);
  unsigned long ei;
  ind_txt_file >> ei;
  ind_txt_file.close();

  boxm2_volm_wr3db_index_sptr ind = new boxm2_volm_wr3db_index((unsigned)(sph_shell->get_container_size()), buffer_capacity());
  ind->initialize_read(ind_file);

  vcl_cout << " " << ei << " hypotheses is read from " << ind_txt << vcl_endl;
  vcl_cout << " Indice is loaded from " << ind_file << vcl_endl;
  //: check the devices
  vcl_cout << " Following devices are being used for volumetric matching:" << vcl_endl;
  for(unsigned i = 0; i < (unsigned)gpus.size(); i++) {
    bocl_device_sptr device = gpus[i];
	vcl_cout << "\t" << device->device_identifier() << vcl_endl;
  }
  //: visualize query
  if(save_images()){
    vcl_string vrml_fname = out_folder() + "cam_hypo_schematic.vrml";
    query->draw_template(vrml_fname);
    query->draw_query_images(out_folder());
  }
  
  vcl_cout << "\n Starting the volm matcher" << vcl_endl;
  //: create the volm_matcher class
  //ei = 1;
  boxm2_volm_matcher matcher(query, ind, ei, gpus);
  //: get the score for all indexes

  volm_io::write_status(out_folder(), volm_io::EXE_STARTED);

  matcher.matching_cost_layer();
  vcl_cout << " Volm matcher finished" << vcl_endl;
  //: save the scores
  vcl_string out_prefix = out_folder() + ind_pre();
  matcher.write_score(out_prefix);
  vcl_string score_fname = out_folder() + ind_pre() + "_score.bin";
  vcl_string cam_fname = out_folder() + ind_pre() + "_camera.bin";
  vcl_cout << " Results are stored in file " << score_fname << " and " << cam_fname << vcl_endl;

  //: composer to generate final output
  vcl_cout << "\n Start to generate output images using tiles" << vcl_endl;
  
  //: read and scale the scores from binary 
  vsl_b_ifstream is(score_fname.c_str());
  vsl_b_ifstream isc(cam_fname.c_str());
  vcl_vector<float> scores;
  vcl_vector<unsigned> cam_ids;
  float score;
  unsigned cam_id;
  for(unsigned i = 0; i < ei; i++) {
	vsl_b_read(is, score);
	vsl_b_read(isc,cam_id);
	scores.push_back(score);
	cam_ids.push_back(cam_id);
    vcl_cout << " for index " << i << ", score = " << score 
		     << ",\t camera id = " << cam_id 
		     << "\t with valid rays = " << query->get_valid_ray_num(cam_id) 
		     << vcl_endl;
  }

  //: generate tile and output imges
  vcl_vector<volm_tile> tiles;
  vcl_vector<vil_image_view<vxl_byte> > out_imgs;
  if(img_category == "desert"){
    tiles = volm_tile::generate_p1_wr1_tiles();
  }else if(img_category == "coast"){
    tiles = volm_tile::generate_p1_tiles();
  }else{
    vcl_cerr << " ERROR: unknow image category regonized, generating tile for output failed\n";
	volm_io::write_status(out_folder(), volm_io::LABELME_FILE_IO_ERROR);
	return volm_io::LABELME_FILE_IO_ERROR;
  }

  vcl_cout << tiles.size() << " tiles/images are generate for query image category " << img_category << vcl_endl;
  for (unsigned i = 0; i < tiles.size(); ++i) {
    vil_image_view<vxl_byte> out(3601, 3601);
    out.fill(volm_io::UNEVALUATED);
	out_imgs.push_back(out);
  }


  //: read in the hypothese
  vcl_string hyp_file = ind_loc() + hypo_file();
  volm_loc_hyp hyp(hyp_file);
  vcl_cout << hyp.size() << " hypotheses read from: " << hyp_file << vcl_endl;

  if( hyp.size() != scores.size() ) {
    vcl_cerr << " ERROR: loaded hypotheses locations is not constistent with indices score, check input hypothese file\n";
	volm_io::write_status(out_folder(), volm_io::EXE_ARGUMENT_ERROR);
	return volm_io::EXE_ARGUMENT_ERROR;
  }

  vgl_point_3d<float> h_pt;
  unsigned start_ind = start();
  unsigned skip_ind = skip();
  while (hyp.get_next(start_ind, skip_ind, h_pt))
  {
	unsigned ind_idx = hyp.current_ - skip_ind;
    vcl_cout << "Processing hypothesis: " << ind_idx << " x: " << h_pt.x() << " y: " << h_pt.y() << " z: " << h_pt.z() 
		     << ", score = " << scores[ind_idx] << ", best cam_id = " << cam_ids[ind_idx]
		     << vcl_endl;
	//: locate the tile/img pixel
	unsigned u, v;
	for(unsigned k = 0; k < tiles.size(); k++){
      if(tiles[k].global_to_img(h_pt.x() , h_pt.y(), u, v)){
        //: write the image at (u,v);
		vbl_array_2d<bool> mask;
        vbl_array_2d<float> kernel;
        volm_tile::get_uncertainty_region((float)10, (float)10, (float)0.01, mask, kernel);
		volm_tile::mark_uncertainty_region(u, v, scores[ind_idx], mask, kernel, out_imgs[k]);
	  }
	}

  }

  //: save images
  for (unsigned i = 0; i < out_imgs.size(); i++) {
    vcl_string out_name = out_folder() + "/VolM_" + tiles[i].get_string() + "_S1x1.png";
	vil_save(out_imgs[i], out_name.c_str());
  }

  volm_io::write_status(out_folder(), volm_io::SUCCESS);
  //vcl_cout << " returning SUCCESS!\n";
  return volm_io::SUCCESS;
}
