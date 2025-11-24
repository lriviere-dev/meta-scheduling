import sys
import os
import re

def clean_column_name(text):
    text = str.lower(text)
    text = str.strip(text)
    text = text.replace(" ", "_")
    return text

def clean_content(text):
    return str.strip(text)

def extract_info(file_name, text): #returns  list of maybe several lines of content
    #split text into general infos/ several experiments
    common_text, experiments_text = re.split(r'Experiment Start', text, maxsplit=1)
    experiments_text = re.split(r"Iteration \d",text)[1:]#stripping the empty one in front
    dicts = []
    expe_counter = 0

    for experiment in experiments_text:
        expe_text = common_text + experiment
        infos = re.findall(r"([^:\n]+):([^\n]+)",expe_text)
        dict = {"log_file" : file_name,
                "expe_number" : str(expe_counter)}
        expe_counter += 1
        for col, content in infos:
            if clean_column_name(col) not in dict :
                dict[clean_column_name(col)] = clean_content(content)
        dicts.append(dict)
    return dicts

def make_lines(cols, contents):
    lines = ";".join(cols)+"\n"
    for d in contents:
        lines += ";".join([d[col] if col in d else "" for col in cols])+"\n"
    return lines

def tabulate_logs(folders): #takes a list of folder
    folders = folders if (type(folders)==list) else [folders]

    result_dirs = [os.getcwd() + "/{}/".format(folder) for folder in folders]
    all_logs = [os.listdir(result_dir) for result_dir in result_dirs]
    contents = []

    print("Extracting data from {} logs...".format("+".join([str(len(x)) for x in all_logs])))

    cols = []
    for i in range(len(folders)):
        for file_name in all_logs[i]:
            with open(result_dirs[i]+file_name, "r") as log:
                log_text = log.read()
                # log_text = re.sub(r"![^é]*----------------+", "", log_text) # special remover for verbose mode on solver ...
                log_content = extract_info(file_name, log_text)
                contents += log_content
                for log_content_i in log_content:
                    cols += [key for key in log_content_i if key not in cols ]


    lines = make_lines(cols, contents)
    print("Writing lines")
    #write resulting file
    output_file = "/{}.csv".format("_".join(folders))
    print(os.getcwd() + output_file)
    with open(os.getcwd() + output_file, "w") as out_file:
        out_file.write(lines)

    print("Done")




if __name__ == "__main__":
    tabulate_logs(["test_output_folder"])