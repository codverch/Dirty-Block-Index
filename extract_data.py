import pandas as pd

# Create an empty dataframe
data = pd.DataFrame(columns=["simSeconds"])

# Loop through the text files
for file_num in range(1, n+1):
    file_name = "m5out/=new_dbi_test_k_4_1_mil.txt".format(file_num)
    with open(file_name) as f:
        lines = f.readlines()
    simSeconds = None
    for line in lines:
        if "sim_seconds" in line:
            simSeconds = float(line.split()[1])
    # Add the extracted data to the dataframe
    data = data.append({"simSeconds": simSeconds}, ignore_index=True)

# Save the dataframe to an Excel file
data.to_excel("output.xlsx", index=False)
