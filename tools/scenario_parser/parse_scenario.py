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
    def read_byte():
        nonlocal offset
        val = struct.unpack_from('<B', data, offset)[0]
        offset += 1
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
    lastFrameIdx = read_int()
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
        "frames": [],
        "results": [],
        "events": []
    }
    
    # Frames
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
                "lanePosition": lanePosRaw / 1000.0,
                "speed": speedRaw / 100.0,
                "hp": hpRaw,
                "temptationMode": temptationMode,
                "blockFrontHorseIndex": blockFrontHorseIndex,
                "distance": distance
            })
        
        # The last frame in the binary doesn't have the 4-byte Time suffix
        # we detect this by checking if f_idx == frameCount - 1
        time_val = 0.0
        if f_idx < frameCount - 1:
            time_val = read_float()
            offset = frame_start + frameSize
        else:
            # Last frame only has horse data (108 bytes)
            offset = frame_start + horseNum * 12
            
        result["frames"].append({"time": time_val, "horses": horses})
        
    # Results
    sizes = [read_int() for _ in range(horseNum)]
    for h, size in enumerate(sizes):
        start_offset = offset
        finishOrder = read_int()
        finishTime = read_float()
        finishTimeRaw = read_float()
        finishDiffTime = read_float()
        runUpTimeRaw = read_float()
        startDelayTime = read_float()
        gutsOrder = read_byte()
        wizOrder = read_byte()
        lastSpurtDist = read_float()
        runningStyle = read_byte()
        defeat = read_int()
        skills = []
        remaining = size - (offset - start_offset)
        skillCount = remaining // 5
        for _ in range(skillCount):
            s_id = read_int()
            s_reason = read_byte()
            skills.append({"skillId": s_id, "reason": s_reason})
        result["results"].append({
            "horseIndex": h,
            "finishOrder": finishOrder,
            "finishTime": finishTime,
            "finishTimeRaw": finishTimeRaw,
            "finishDiffTime": finishDiffTime,
            "runUpTimeRaw": runUpTimeRaw,
            "startDelayTime": startDelayTime,
            "gutsOrder": gutsOrder,
            "wizOrder": wizOrder,
            "lastSpurtStartDistance": lastSpurtDist,
            "runningStyle": runningStyle,
            "defeat": defeat,
            "noActivateSkills": skills
        })
        offset = start_offset + size
        
    # Events
    ev_unknown = read_int()
    ev_count = read_int()
    for i in range(ev_count):
        frame_idx = read_int()
        dist_short = read_short()
        ev_type = read_byte()
        param_len = read_byte()
        params = [read_int() for _ in range(param_len)]
        activate_count = read_int()
        unk = read_int()
        bool_val = read_byte()
        
        result["events"].append({
            "frame_idx": frame_idx,
            "dist_short": dist_short,
            "type": ev_type,
            "params": params,
            "activate_count": activate_count,
            "unk_float_time_or_int": unk,
            "bool_val": bool_val
        })
        
    with open('/root/parsed_scenario.json', 'w') as out:
        json.dump(result, out, indent=2)
    print("Parsed FULL scenario successfully! JSON saved to /root/parsed_scenario.json")

parse_scenario('/root/decode_scenario.bin')
