import json
import ijson
from decimal import Decimal
from bokeh.plotting import figure, output_file, show
from bokeh.layouts import column
from bokeh.palettes import Set1

def convert_decimals(obj):
    """
    Recursively convert Decimal objects to float in the given object.
    """
    if isinstance(obj, list):
        return [convert_decimals(item) for item in obj]
    elif isinstance(obj, dict):
        return {key: convert_decimals(value) for key, value in obj.items()}
    elif isinstance(obj, Decimal):
        return float(obj)
    else:
        return obj

def stream_large_json_file(filename):
    """
    Stream JSON objects one by one from a large file.
    """
    with open(filename, 'r') as f:
        for item in ijson.items(f, 'item'):
            yield item

def stream_animal_data(static_filename, dynamic_filename):
    """
    Stream animal data from static and dynamic JSON files.
    """
    # Load static data first
    species_details = {}
    with open(static_filename, 'r') as static_file:
        static_data = convert_decimals(json.load(static_file))
        for animal in static_data:
            species_details[animal['id']] = {
                'species_name': animal['species_name'],
                'is_herbivore': animal['is_herbivore'],
                'speed': animal['speed'],
                'stealth_level': animal['stealth_level'],
                'detection_skill': animal['detection_skill'],
                'detection_range': animal['detection_range']
            }

    # Initialize statistics
    total_herbivores = []
    total_carnivores = []
    hunger_herbivores = []
    health_herbivores = []
    hunger_carnivores = []
    health_carnivores = []
    species_populations = {}
    stealth_levels_over_time = {}
    detection_skills_over_time = {}
    speed_over_time = {}

    # Process dynamic data frame by frame
    for step_data in stream_large_json_file(dynamic_filename):
        step_data = convert_decimals(step_data)
        frame_species_pop = {}
        frame_species_stealth = {}
        frame_species_detection = {}
        frame_species_speed = {}

        frame_herbivores = 0
        frame_carnivores = 0
        frame_hunger_herbivores = 0
        frame_health_herbivores = 0
        frame_hunger_carnivores = 0
        frame_health_carnivores = 0
        herbivore_count = 0
        carnivore_count = 0

        for animal in step_data['data']:
            species_info = species_details.get(animal['id'], {})
            species_name = species_info.get('species_name', 'Unknown')
            is_herbivore = species_info.get('is_herbivore', False)

            hunger = animal['hunger']
            health = animal['health']

            if is_herbivore:
                frame_herbivores += 1
                frame_hunger_herbivores += hunger
                frame_health_herbivores += health
                herbivore_count += 1
            else:
                frame_carnivores += 1
                frame_hunger_carnivores += hunger
                frame_health_carnivores += health
                carnivore_count += 1

            if species_name not in frame_species_pop:
                frame_species_pop[species_name] = 0
                frame_species_stealth[species_name] = []
                frame_species_detection[species_name] = []
                frame_species_speed[species_name] = []

            frame_species_pop[species_name] += 1
            frame_species_stealth[species_name].append(species_info['stealth_level'])
            frame_species_detection[species_name].append(species_info['detection_skill'])
            frame_species_speed[species_name].append(species_info['speed'])

        # Append overall stats for this frame
        total_herbivores.append(frame_herbivores)
        total_carnivores.append(frame_carnivores)
        hunger_herbivores.append(frame_hunger_herbivores / herbivore_count if herbivore_count else 0)
        health_herbivores.append(frame_health_herbivores / herbivore_count if herbivore_count else 0)
        hunger_carnivores.append(frame_hunger_carnivores / carnivore_count if carnivore_count else 0)
        health_carnivores.append(frame_health_carnivores / carnivore_count if carnivore_count else 0)

        # Update species-specific stats
        for species_name, count in frame_species_pop.items():
            if species_name not in species_populations:
                species_populations[species_name] = []
                stealth_levels_over_time[species_name] = []
                detection_skills_over_time[species_name] = []
                speed_over_time[species_name] = []

            species_populations[species_name].append(count)
            stealth_levels_over_time[species_name].append(
                sum(frame_species_stealth[species_name]) / len(frame_species_stealth[species_name])
            )
            detection_skills_over_time[species_name].append(
                sum(frame_species_detection[species_name]) / len(frame_species_detection[species_name])
            )
            speed_over_time[species_name].append(
                sum(frame_species_speed[species_name]) / len(frame_species_speed[species_name])
            )

    return {
        'species_populations': species_populations,
        'total_herbivores': total_herbivores,
        'total_carnivores': total_carnivores,
        'hunger_herbivores': hunger_herbivores,
        'health_herbivores': health_herbivores,
        'hunger_carnivores': hunger_carnivores,
        'health_carnivores': health_carnivores,
        'stealth_levels_over_time': stealth_levels_over_time,
        'detection_skills_over_time': detection_skills_over_time,
        'speed_over_time': speed_over_time,
    }

def stream_plant_data(plant_filename):
    """
    Stream plant data from the JSON file and calculate average food level over time.
    """
    avg_food_levels = []

    for step_data in stream_large_json_file(plant_filename):
        step_data = convert_decimals(step_data)
        total_food = 0
        plant_count = 0

        for plant in step_data['plants']:
            total_food += plant['food']
            plant_count += 1

        avg_food_levels.append(total_food / plant_count if plant_count > 0 else 0)

    return avg_food_levels

# Specify the file paths
num = '1'

str_num = str(num)
path_prefix = 'C:\\Users\\Doruk\\env\\tubitak2025\\json\\' + str_num +'\\'
static_filename = path_prefix + 'animal_static_data.json'
dynamic_filename = path_prefix + 'animal_dynamic_data.json'
plant_filename = path_prefix + 'plant_data1.json'

# Process the data
stats = stream_animal_data(static_filename, dynamic_filename)
avg_plant_food_levels = stream_plant_data(plant_filename)

# Setup output file for Bokeh
output_file(path_prefix + "animal_evolution_with_plants.html")

# Create a figure for herbivore and carnivore populations with plant food levels
fig1 = figure(title="Herbivore, Carnivore Populations and Plant Food Levels Over Time", x_axis_label="Frame", y_axis_label="Count/Food Level", width=1300, height=600)
fig1.line(list(range(len(stats['total_herbivores']))), stats['total_herbivores'], legend_label="Herbivores", line_width=2, color="green")
fig1.line(list(range(len(stats['total_carnivores']))), stats['total_carnivores'], legend_label="Carnivores", line_width=2, color="red")
fig1.line(list(range(len(avg_plant_food_levels))), avg_plant_food_levels, legend_label="Average Plant Food Levels", line_width=2, color="blue")

# Create a figure for average hunger and health trends by type
fig2 = figure(title="Herbivore and Carnivore Health and Hunger Trends", x_axis_label="Frame", y_axis_label="Value", width=1300, height=600)
fig2.line(list(range(len(stats['hunger_herbivores']))), stats['hunger_herbivores'], legend_label="Herbivore Hunger", line_width=2, color="lightgreen")
fig2.line(list(range(len(stats['health_herbivores']))), stats['health_herbivores'], legend_label="Herbivore Health", line_width=2, color="darkgreen")
fig2.line(list(range(len(stats['hunger_carnivores']))), stats['hunger_carnivores'], legend_label="Carnivore Hunger", line_width=2, color="pink")
fig2.line(list(range(len(stats['health_carnivores']))), stats['health_carnivores'], legend_label="Carnivore Health", line_width=2, color="red")

# Create a figure for trait evolution
fig3 = figure(title="Trait Evolution Over Time (Stealth, Detection Skill, Speed)", x_axis_label="Frame", y_axis_label="Trait Value", width=1300, height=600)
colors = Set1[9]  # Set of distinct colors
for idx, (species_name, stealth_levels) in enumerate(stats['stealth_levels_over_time'].items()):
    fig3.line(list(range(len(stealth_levels))), stealth_levels, legend_label=f"{species_name} Avg Stealth", line_width=2, color=colors[idx % len(colors)], line_dash="dashed")
for idx, (species_name, speeds) in enumerate(stats['speed_over_time'].items()):
    fig3.line(list(range(len(speeds))), speeds, legend_label=f"{species_name} Avg Speed", line_width=2, color=colors[idx % len(colors)], line_dash="solid")
for idx, (species_name, detection_skills) in enumerate(stats['detection_skills_over_time'].items()):
    fig3.line(list(range(len(detection_skills))), detection_skills, legend_label=f"{species_name} Avg Detection Skill", line_width=2, color=colors[idx % len(colors)], line_dash="dotdash")

# Create a figure for population trends of each species
fig4 = figure(title="Population Trends by Species Over Time", x_axis_label="Frame", y_axis_label="Population", width=1300, height=600)
for idx, (species_name, populations) in enumerate(stats['species_populations'].items()):
    fig4.line(list(range(len(populations))), populations, legend_label=f"{species_name} Population", line_width=2, color=colors[idx % len(colors)])

# Layout for Bokeh visualization
layout = column(fig1, fig2, fig3, fig4)

# Show the plots
show(layout)
