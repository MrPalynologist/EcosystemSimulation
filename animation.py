import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import os
import json
from matplotlib.patches import Rectangle
from matplotlib.animation import FFMpegWriter

num = 19;
str_num = str(num);
# Dosya yolunu belirtin
path_prefix = 'C:\\Users\\keles\\env\\tubitak2025\\json\\' + str_num + '\\'

# Gerçek ffmpeg yolunuzu güncelleyin
os.environ['PATH'] += r";C:\Users\keles\Masaüstü\ffmpeg-2024-10-07-git-496b8d7a13-full_build\ffmpeg-2024-10-07-git-496b8d7a13-full_build\bin"

def read_json_file(filename):
    with open(filename, 'r') as f:
        return json.load(f)

# Dosya yolları
static_animal_filename = path_prefix + 'animal_static_data.json'
dynamic_animal_filename = path_prefix + 'animal_dynamic_data.json'
plant_filename = path_prefix + 'plant_data1.json'

# Önce statik hayvan verilerini yükle
def load_static_animal_data(static_filename):
    with open(static_filename, 'r') as f:
        static_data = json.load(f)
    # Hayvan kimliklerini statik özellikleriyle eşleştirmek için bir sözlük oluştur
    static_animal_info = {}
    for animal in static_data:
        static_animal_info[animal['id']] = {
            'species_name': animal['species_name'],
            'is_herbivore': animal['is_herbivore']
        }
    return static_animal_info

# Verileri yükle
static_animal_info = load_static_animal_data(static_animal_filename)
dynamic_animal_data = read_json_file(dynamic_animal_filename)
plant_data = read_json_file(plant_filename)

# Grafiği hazırla
fig, ax = plt.subplots(figsize=(10, 10))
ax.set_xlim(0, 4000)  # Simülasyon alanınıza göre sınırları ayarlayın
ax.set_ylim(0, 4000)
ax.set_aspect('equal')

# Boş saçılma grafikleri başlat
animal_scatter = ax.scatter([], [], s=[], edgecolors='black', linewidths=1.5)
plant_scatter = ax.scatter([], [], s=10, alpha=0.6, color='green')

# Animasyon için güncelleme fonksiyonu
def update(frame):
    x_animals, y_animals = [], []
    facecolors_animals, edgecolors_animals, sizes_animals = [], [], []
    x_plants, y_plants, sizes_plants = [], [], []

    # Hayvanları işle
    if frame < len(dynamic_animal_data) and 'data' in dynamic_animal_data[frame]:
        for animal in dynamic_animal_data[frame]['data']:
            # Bu hayvan için statik bilgiyi al
            static_info = static_animal_info.get(animal['id'], {})
            
            x_animals.append(animal['x'])
            y_animals.append(animal['y'])
            
            # Statik veriye göre otçul durumuna bağlı renk
            facecolors_animals.append('green' if static_info.get('is_herbivore', False) else 'red')
            edgecolors_animals.append('black')
            
            # Sağlığa göre boyut
            sizes_animals.append(animal['health'] / 2)

    # Bitkileri işle
    if frame < len(plant_data):
        for plant in plant_data[frame]['plants']:
            x_plants.append(plant['x'])
            y_plants.append(plant['y'])
            sizes_plants.append(plant['food'] / 5)

    # Saçılma grafiklerini güncelle
    animal_scatter.set_offsets(np.c_[x_animals, y_animals])
    animal_scatter.set_facecolor(facecolors_animals)
    animal_scatter.set_edgecolor(edgecolors_animals)
    animal_scatter.set_sizes(sizes_animals)

    plant_scatter.set_offsets(np.c_[x_plants, y_plants])
    plant_scatter.set_sizes(sizes_plants)

    # Geçerli kareyi göstermek için başlık ayarla
    ax.set_title(f'Frame: {frame}')

    return animal_scatter, plant_scatter

# Toplam kare sayısı, en uzun veri dizisine göre
num_frames = max(len(dynamic_animal_data), len(plant_data))

# Animasyonu oluştur
anim = animation.FuncAnimation(fig, update, frames=num_frames, interval=50)

# Animasyonu kaydet
writer = FFMpegWriter(fps=30)
anim.save('simulation_animation.mp4', writer=writer)

plt.close(fig)
print("Video 'simulation_animation.mp4' olarak kaydedildi")
