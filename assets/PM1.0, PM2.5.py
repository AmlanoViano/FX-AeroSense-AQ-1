import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file_path = r"C:\Users\ROG\Desktop\Data\data.csv"

df = pd.read_csv(file_path)

print(df)

#Line chart for showing trends of PM1.0, PM2.5

plt.figure(figsize=(10,5))
plt.plot(df["Timestamp"], df["PM1.0"], label="PM1.0")
plt.plot(df["Timestamp"], df["PM2.5"], label="PM2.5")
plt.xlabel("Time (s)")
plt.ylabel("PM Concentration")
plt.title("PM1.0 & PM2.5 Over Time")
plt.legend()
plt.show()