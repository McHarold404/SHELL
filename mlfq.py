import matplotlib.pyplot as plt 

f = open("logs.txt", "r")

dic = {} 
ticks = {}

for line in f.readlines():
	vals = line.split()
	if len(vals)<1:
		continue
	if vals[0] == "ToRead" :
		tick = int(vals[1])
		pid = int(vals[2])
		queue = int(vals[3])

		if pid not in dic :
			dic[pid] = []
			ticks[pid] = []

		ticks[pid].append(tick)
		dic[pid].append(queue)

		# print(tick, pid, queue)

fig, ax = plt.subplots()

for pid in dic:
	ax.plot(ticks[pid], dic[pid])
	# print pid, ticks[:10], dic[pid][:10]

ax.set_title("Multi-level Feedback Queue time plot")
ax.xaxis.set_label_text('Time elapsed (ticks)')
ax.yaxis.set_label_text('Queue ID')

legends = []
for pid in dic :
	text = str(pid)
	text = "P" + text 
	legends.append(text)

ax.legend(legends)

# plt.show()
plt.savefig('graph.png')
plt.close(fig)