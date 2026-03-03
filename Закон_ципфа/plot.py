import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("../data/zipf_stats.csv")
df = df.head(1000)

C = df['frequency'].iloc[0]
df['theoretical'] = C / df['rank']

plt.figure(figsize=(10, 6))
plt.loglog(df['rank'], df['frequency'], 'b-', label='Фактическая частота')
plt.loglog(df['rank'], df['theoretical'], 'r--', label='Теоретическая (C/r)')
plt.xlabel('Ранг')
plt.ylabel('Частота')
plt.title('Закон Ципфа')
plt.legend()
plt.grid(True, which="both", ls="-")
plt.savefig("../data/zipf_plot.png", dpi=150)
plt.show()