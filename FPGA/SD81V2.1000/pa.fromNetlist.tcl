
# PlanAhead Launch Script for Post-Synthesis floorplanning, created by Project Navigator

create_project -name SD81V2.1000 -dir "/home/ise/ClaudeCode/SD81-Booster/FPGA/SD81V2.1000/planAhead_run_4" -part xc6slx9tqg144-3
set_property design_mode GateLvl [get_property srcset [current_run -impl]]
set_property edif_top_file "/home/ise/ClaudeCode/SD81-Booster/FPGA/SD81V2.1000/SD81.ngc" [ get_property srcset [ current_run ] ]
add_files -norecurse { {/home/ise/ClaudeCode/SD81-Booster/FPGA/SD81V2.1000} {ipcore_dir} }
add_files [list {ipcore_dir/bramdp_w.ncf}] -fileset [get_property constrset [current_run]]
set_property target_constrs_file "SD81XC6.ucf" [current_fileset -constrset]
add_files [list {SD81XC6.ucf}] -fileset [get_property constrset [current_run]]
link_design
