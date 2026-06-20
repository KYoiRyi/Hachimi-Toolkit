import struct
import json

def parse_scenario(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    
    offset = 0
    def read_int():
        nonlocal offset
        val = struct.unpack_from('<i', data, offset)[0]
        offset += 4
        return val
    def read_float():
        nonlocal offset
        val = struct.unpack_from('<f', data, offset)[0]
        offset += 4
        return val
    def read_short():
        nonlocal offset
        val = struct.unpack_from('<h', data, offset)[0]
        offset += 2
        return val
    def read_sbyte():
        nonlocal offset
        val = struct.unpack_from('<b', data, offset)[0]
        offset += 1
        return val
        
    header_len = read_int()
    version = read_int()
    
    distanceDiffMax = read_float()
    horseNum = read_int()
    lastFrameIdx = read_int() # unknown exactly
    unknown3 = read_int()
    frameCount = read_int() 
    frameSize = read_int()
    unknown6 = read_int()
    unknown7 = read_int()
    
    result = {
        "version": version,
        "distanceDiffMax": distanceDiffMax,
        "horseNum": horseNum,
        "frameCount": frameCount,
        "frameSize": frameSize,
        "frames": []
    }
    
    for f_idx in range(frameCount):
        frame_start = offset
        
        horses = []
        for h in range(horseNum):
            lanePosRaw = read_short()
            speedRaw = read_short()
            hpRaw = read_short()
            temptationMode = read_sbyte()
            blockFrontHorseIndex = read_sbyte()
            distance = read_float()
            
            horses.append({
                "horseIndex": h,
                "lanePosition": lanePosRaw / 1000.0, # Guessing scale
                "speed": speedRaw / 100.0,           # Guessing scale
                "hp": hpRaw,                         # Scaled or raw?
                "temptationMode": temptationMode,
                "blockFrontHorseIndex": blockFrontHorseIndex,
                "distance": distance
            })
        
        time_val = read_float()
        
        result["frames"].append({
            "time": time_val,
            "horses": horses
        })
        
        offset = frame_start + frameSize
        
    # Write to JSON for the user to view
    with open('/root/parsed_scenario.json', 'w') as out:
        json.dump(result, out, indent=2)
    print("Parsed successfully! JSON saved to /root/parsed_scenario.json")

parse_scenario('/root/decode_scenario.bin')
