import os
from itertools import product

# Slurm and display parameters
run_name = "var_test"  # Change this to your run name
experiment_mode = 'star'  # "product" for full combinorial of all parameters or "star" to vary only one parameter at a time (note that default param aren't used if product mode)
OUTDIR = "outputs_" + run_name  
ERRDIR = "errors_" + run_name  
MAILTO = "louis.riviere@laas.fr"  # Mail to mail when experiment ends
MAXMEM = "4000"  # Memory (mega)
MAXTIME = "10:00:00"  # Time limit

# Parameters for experiments: (values list, default value)
benchfolders = [os.getcwd() + "/instances/" + name for name in ["bench_1p_var"]] #list all folders where to look for instance files

parameters = {  # parameter_name : [values], default value (for star mode)
    "jseq_time": ([60*60], 60*60), 
    "nb_sampled_scenarios": ([5, 25, 100], 25), 
}


# Create directories
os.system(f"mkdir -p {OUTDIR}")
os.system(f"mkdir -p {ERRDIR}")


# Open keyfile to write commands
job_lines = []
with open(f'{run_name}.key', 'w') as keyfile:

    # Generate parameter combinations based on the selected mode
    param_combinations = []
    param_keys = list(parameters.keys())

    if experiment_mode == 'product':
        # 'product' mode: Generate all combinations of parameters
        param_values_lists = [param[0] for param in parameters.values()]  # Get the possible values lists
        param_combinations = product(*param_values_lists)
    elif experiment_mode == 'star':
        # 'star' mode: Vary one parameter at a time while keeping others at default values
        param_combinations = []
        for i, param_key in enumerate(param_keys):
            # Set the other parameters to their default values
            param_values = {key: (parameters[key][0] if key == param_key else [parameters[key][1]]) for key in param_keys}
            param_combinations.extend(product(*param_values.values()))

    # Write the job commands to the keyfile
    for param_values in param_combinations:
        param_dict = dict(zip(param_keys, param_values))
        for benchfolder in benchfolders:
            for filename in os.listdir(benchfolder):
                if filename.endswith('.data'):
                    # Construct the full path for the .data file
                    file_path = os.path.join(benchfolder, filename)
                    # Write the job command for the job file with instance name first
                    job_line = f"./{run_name}_main {file_path} {' '.join(map(str, param_values))}\n"
                    keyfile.write(job_line)
                    job_lines.append(job_line)

# Calculate the number of jobs
num_jobs = len(job_lines)

# Create job file for SLURM batch system
with open(f'jobs_{run_name}.sh', 'w') as slurm_file:
    slurm_file.write("#!/bin/sh\n")
    slurm_file.write(f"#SBATCH  --exclude=trencavel-10g,balibaste-10g,nestorius-10g,spinoza-10g,perelha,dolcino,askew-10g,pirovano,molay,sernay,hus --ntasks=1 --mail-type=END --mail-user={MAILTO} --mem={MAXMEM} --array=1-{num_jobs} --time={MAXTIME} --output {OUTDIR}/{run_name}_%a.out --error {ERRDIR}/{run_name}_%a.err\n")
    slurm_file.write(f"srun -u `sed ${{SLURM_ARRAY_TASK_ID}}'q;d' {run_name}.key`\n")

# Output the SLURM job script path and jobs file path
print(f"Job submission script generated: jobs_{run_name}.sh")
print(f"Job commands written to: {run_name}.key")
