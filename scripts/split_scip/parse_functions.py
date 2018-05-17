#! /usr/bin/env python

import clang.cindex
import sys
import clang
from clang.cindex import CursorKind, TokenKind, StorageClass

clang.cindex.Config.set_library_file("/usr/lib/llvm-5.0/lib/libclang.so.1")
human_readable=False

def fully_qualified(c):
    if c is None:
        return ''
    elif c.kind == CursorKind.TRANSLATION_UNIT:
        return ''
    else:
        res = fully_qualified(c.semantic_parent)
        if res != '':
            return res + '::' + c.spelling
    return c.spelling

def output(name, start, end):
    if human_readable:
        print "{} {} {}".format(name, start, end)
    else:
        print "{},{}p".format(start,end)


def recurse_into_static_functions(cursor, static_functions_dict):
    for d in cursor.walk_preorder():
        if d.kind == CursorKind.CALL_EXPR and d.referenced.storage_class == StorageClass.STATIC:
                                #print "\t->{}".format(fully_qualified(d.referenced))
            if d.referenced.extent.start.line not in static_functions_dict:
                static_functions_dict[d.referenced.extent.start.line] = d.referenced
                recurse_into_static_functions(d.referenced, static_functions_dict)


if __name__ == "__main__":
    idx = clang.cindex.Index.create()
    tu = idx.parse(sys.argv[1])

    function_dict = {}
    kindtypes = dict()
    static_functions_dict = dict()
    for idx, c in enumerate(tu.cursor.walk_preorder()):

        kindtypes[c.kind] = kindtypes.get(c.kind, 0) + 1

        if c.kind == CursorKind.FUNCTION_DECL and c.get_definition() is not None:
            #print("\n".join(dir(c)))
            #sys.exit(0)
            if str(c.location.file).endswith("c"):

                if c.storage_class == StorageClass.STATIC:
                    #print "{} : {}--{}".format(fully_qualified(c.referenced), c.extent.start.line,
                    #
                    #c.extent.end.line)
                    pass

                with open(sys.argv[2], "r") as headerfile:
                    if "{}(".format(c.referenced.spelling) in headerfile.read():
                        #print "{} : {}--{}".format(fully_qualified(c.referenced), c.extent.start.line, c.extent.end.line)

                        function_dict[c.extent.start.line] = c

                        recurse_into_static_functions(c, static_functions_dict)



    #print "\n".join(map(str, kindtypes.items()))
    #
    # loop over documentation that and keep merge tokens that are doxygen comments directly before
    # the start of a new function declaration
    #
    gaps = {173:173, 185:185, 194:194, 207:211, 222:222, 233:233, 244:244, 255:255, 266:266, 301:301, 334:334, 344:394, 418:418, 470:474, 490:490, 581:581, 603:603, 675:675, 688:688, 704:704, 774:774, 790:790, 801:801, 816:816, 833:833, 846:850, 893:893, 904:904, 916:916, 928:928, 944:944, 971:971, 987:987, 1004:1004, 1021:1021, 1040:1040, 1056:1094, 1195:1195, 1280:1280, 1350:1350, 1435:1436, 1498:1498, 1553:1553, 1583:1583, 1620:1620, 1877:1877, 2045:2045, 2102:2102, 2158:2158, 2219:2219, 2368:2368, 2570:2570, 2719:2720, 2766:2766, 2858:2858, 3014:3014, 3204:3204, 3253:3253, 3287:3287, 3310:3496, 3574:3574, 3663:3664, 3738:3738, 3825:3880, 3914:3914, 3970:3975, 4001:4001, 4029:4029, 4057:4057, 4085:4085, 4112:4112, 4138:4138, 4153:4153, 4168:4168, 4187:4187, 4206:4206, 4225:4225, 4244:4244, 4263:4263, 4282:4282, 4303:4303, 4321:4321, 4340:4340, 4366:4366, 4385:4385, 4398:4398, 4424:4424, 4443:4443, 4456:4456, 4482:4482, 4501:4501, 4514:4514, 4540:4540, 4559:4559, 4572:4572, 4598:4598, 4617:4617, 4630:4630, 4656:4656, 4675:4675, 4688:4688, 4706:4706, 4729:4729, 4751:4751, 4769:4769, 4786:4786, 4816:4816, 4837:4837, 4863:4863, 4889:4889, 4915:4915, 4929:4929, 4943:4950, 4993:4993, 5035:5035, 5059:5059, 5083:5083, 5107:5107, 5131:5131, 5144:5144, 5155:5155, 5166:5166, 5222:5222, 5278:5278, 5302:5302, 5326:5326, 5350:5350, 5374:5374, 5398:5398, 5422:5422, 5435:5435, 5448:5448, 5459:5459, 5470:5470, 5485:5485, 5508:5508, 5529:5529, 5602:5602, 5657:5657, 5681:5681, 5705:5705, 5729:5729, 5753:5753, 5777:5777, 5801:5801, 5825:5825, 5849:5849, 5873:5873, 5913:5913, 5937:5937, 5950:5950, 5963:5963, 5974:5974, 5985:5985, 6011:6011, 6032:6032, 6046:6046, 6087:6087, 6123:6123, 6161:6161, 6176:6176, 6201:6201, 6237:6237, 6277:6277, 6315:6315, 6353:6353, 6371:6371, 6424:6424, 6474:6474, 6498:6498, 6522:6522, 6546:6546, 6570:6570, 6594:6594, 6618:6618, 6656:6656, 6689:6689, 6722:6722, 6811:6811, 6874:6874, 6920:6920, 6962:6962, 6986:6986, 7011:7011, 7035:7035, 7059:7059, 7083:7083, 7107:7107, 7131:7131, 7155:7155, 7179:7179, 7217:7217, 7240:7240, 7263:7263, 7286:7286, 7309:7309, 7332:7332, 7355:7355, 7378:7378, 7401:7401, 7424:7424, 7447:7447, 7470:7470, 7493:7493, 7516:7516, 7551:7551, 7562:7562, 7573:7573, 7615:7615, 7654:7654, 7670:7670, 7686:7686, 7702:7702, 7718:7718, 7734:7734, 7750:7750, 7763:7763, 7776:7776, 7787:7787, 7802:7802, 7845:7845, 7886:7886, 7902:7902, 7918:7918, 7934:7934, 7950:7950, 7966:7966, 7982:7982, 7995:7995, 8008:8008, 8019:8019, 8034:8034, 8076:8076, 8116:8116, 8132:8132, 8148:8148, 8164:8164, 8180:8180, 8196:8196, 8212:8213, 8226:8226, 8239:8239, 8250:8250, 8265:8265, 8313:8313, 8361:8361, 8377:8377, 8393:8393, 8409:8409, 8425:8425, 8441:8441, 8457:8457, 8470:8470, 8483:8483, 8494:8494, 8509:8511, 8533:8534, 8586:8586, 8629:8629, 8645:8645, 8661:8661, 8677:8677, 8693:8693, 8709:8709, 8725:8725, 8741:8741, 8757:8757, 8790:8790, 8806:8807, 8820:8820, 8833:8833, 8844:8844, 8859:8859, 8874:8874, 8921:8921, 8934:8934, 8945:8945, 8956:8956, 9011:9011, 9061:9063, 9079:9079, 9095:9095, 9111:9111, 9127:9127, 9143:9143, 9159:9159, 9172:9172, 9185:9185, 9196:9196, 9211:9211, 9252:9252, 9295:9297, 9313:9313, 9329:9329, 9345:9345, 9361:9361, 9377:9377, 9393:9393, 9406:9406, 9419:9419, 9430:9430, 9445:9445, 9499:9499, 9540:9540, 9578:9578, 9592:9592, 9606:9606, 9620:9620, 9634:9634, 9648:9648, 9662:9662, 9676:9676, 9689:9689, 9700:9700, 9711:9711, 9754:9754, 9796:9796, 9812:9812, 9828:9828, 9844:9844, 9860:9860, 9876:9876, 9892:9892, 9905:9905, 9916:9916, 9927:9927, 9942:9942, 9957:9957, 9968:9968, 10015:10015, 10058:10058, 10074:10074, 10090:10090, 10106:10106, 10122:10122, 10138:10138, 10154:10156, 10172:10172, 10188:10188, 10204:10204, 10219:10219, 10230:10230, 10241:10241, 10256:10256, 10271:10271, 10286:10286, 10328:10328, 10341:10341, 10352:10352, 10363:10363, 10376:10376, 10387:10387, 10425:10425, 10438:10438, 10449:10449, 10460:10475, 10511:10511, 10524:10524, 10537:10537, 10548:10548, 10563:10563, 10580:10580, 10591:10591, 10605:10605, 10616:10616, 10641:10646, 10680:10680, 10694:10694, 10711:10711, 10728:10728, 10745:10745, 10758:10758, 10779:10779, 10796:10796, 10813:10813, 10848:10852, 10910:10910, 10945:10945, 10966:10966, 10987:10987, 11008:11008, 11030:11030, 11051:11051, 11072:11072, 11249:11343, 11390:11390, 11437:11437, 11536:11537, 11713:11713, 11762:11762, 11816:11816, 11843:11843, 11872:11872, 11974:11974, 12001:12001, 12027:12027, 12049:12049, 12070:12070, 12095:12095, 12120:12120, 12143:12143, 12166:12166, 12243:12243, 12268:12268, 12311:12311, 12393:12393, 12421:12421, 12493:12493, 12541:12541, 12609:12609, 12687:12687, 12739:12739, 12784:12784, 12829:12829, 12874:12874, 12919:12919, 12964:12965, 13011:13012, 13057:13057, 13100:13100, 13148:13148, 13177:13177, 13204:13204, 13231:13231, 13258:13258, 13285:13285, 13312:13312, 13364:13365, 13431:13431, 13489:13489, 13516:13516, 13592:13592, 13644:13644, 13693:13693, 13750:13750, 13791:13791, 13834:13834, 13880:13880, 13907:13907, 13934:13934, 13976:13980, 14043:14043, 14070:14070, 14137:14137, 14174:14174, 14218:14218, 14286:14286, 14305:14305, 14325:14325, 14344:14344, 14364:14364, 14381:14381, 14398:14398, 14449:14449, 14492:14492, 14514:14514, 14549:14549, 14573:14579, 14724:14724, 14735:14735, 14747:14747, 14759:14759, 14771:14771, 14783:14783, 14791:14791, 14799:14881, 15122:15794, 16043:16248, 16345:16345, 16462:16462, 16643:16906, 17064:17064, 17348:17348, 17380:17380, 17566:17566, 17635:17635, 17661:17661, 17680:17680, 17725:17725, 17757:17757, 17789:17789, 17804:17804, 17819:17819, 17832:17832, 17862:17862, 17890:17890, 17925:17925, 17947:17947, 17977:17977, 18006:18006, 18039:18039, 18072:18072, 18139:18139, 18202:18202, 18273:18273, 18306:18306, 18332:18332, 18342:18342, 18368:18368, 18384:18384, 18411:18411, 18428:18428, 18460:18460, 18486:18486, 18512:18512, 18537:18537, 18562:18567, 18643:18643, 18686:18686, 18742:18742, 18794:18794, 18855:18855, 18933:18933, 18993:18993, 19063:19063, 19162:19162, 19270:19270, 19632:19632, 19684:19684, 19715:19715, 19778:19778, 19821:19821, 19860:19860, 19908:19908, 19947:19947, 19995:19995, 20029:20029, 20067:20067, 20114:20114, 20167:20167, 20194:20194, 20260:20260, 20299:20299, 20341:20341, 20386:20386, 20432:20433, 20476:20476, 20612:20612, 20748:20748, 20768:20768, 20786:20786, 20805:20805, 20842:20842, 20880:20880, 20916:20916, 20957:20957, 21006:21006, 21025:21025, 21048:21048, 21070:21070, 21099:21099, 21125:21125, 21149:21149, 21211:21211, 21311:21381, 21476:21784, 22102:22102, 22193:22193, 22304:22304, 22417:22417, 22429:22429, 22466:22466, 22501:22501, 22570:22571, 22604:22604, 22638:22638, 22670:22670, 22724:22724, 22760:22760, 22845:22845, 22928:22928, 22976:22976, 23020:23020, 23052:23052, 23084:23084, 23174:23174, 23263:23263, 23307:23307, 23351:23351, 23438:23438, 23525:23525, 23558:23558, 23591:23591, 23707:23707, 23823:23823, 23887:23887, 24000:24000, 24113:24113, 24215:24215, 24279:24279, 24393:24393, 24507:24507, 24610:24610, 24730:24730, 24850:24856, 24877:24877, 24898:24898, 24919:24919, 24940:24940, 24955:24955, 24970:24970, 24985:24985, 25000:25000, 25024:25024, 25047:25047, 25106:25106, 25165:25165, 25296:25296, 25335:25622, 25848:25848, 25905:25906, 25947:25947, 25974:25974, 26001:26001, 26028:26028, 26079:26080, 26234:26234, 26268:26268, 26297:26297, 26325:26325, 26353:26353, 26397:26397, 26430:26430, 26460:26460, 26491:26547, 26648:26648, 26746:26746, 26888:26888, 26933:26933, 26943:26943, 26953:26953, 26966:26966, 26980:26980, 26990:26990, 27001:27001, 27034:27034, 27053:27053, 27072:27072, 27104:27104, 27130:27130, 27158:27158, 27184:27184, 27212:27212, 27238:27238, 27266:27266, 27293:27293, 27314:27314, 27348:27348, 27376:27376, 27393:27393, 27429:27429, 27467:27467, 27499:27499, 27531:27531, 27562:27562, 27593:27593, 27624:27624, 27655:27655, 27681:27681, 27707:27707, 27735:27735, 27763:27763, 27794:27794, 27825:27825, 27896:27896, 27961:27961, 27989:27989, 28017:28017, 28048:28048, 28079:28079, 28123:28123, 28167:28167, 28201:28205, 28228:28228, 28254:28254, 28286:28286, 28321:28321, 28353:28353, 28389:28389, 28422:28422, 28458:28458, 28496:28496, 28524:28524, 28548:28548, 28572:28572, 28604:28604, 28643:28647, 28733:28733, 28795:28795, 28826:28826, 28888:28888, 28933:28933, 28958:28958, 28983:28983, 29008:29008, 29033:29033, 29058:29058, 29085:29085, 29111:29111, 29136:29136, 29161:29161, 29186:29186, 29242:29242, 29282:29282, 29329:29329, 29367:29367, 29413:29413, 29442:29442, 29470:29470, 29498:29498, 29523:29523, 29557:29557, 29582:29582, 29610:29610, 29637:29637, 29667:29669, 29702:29702, 29730:29730, 29761:29761, 29793:29793, 29826:29826, 29857:29857, 29887:29887, 29919:29919, 29947:29947, 29974:29974, 30003:30003, 30032:30032, 30070:30070, 30131:30131, 30159:30159, 30187:30187, 30223:30223, 30271:30271, 30314:30318, 30336:30336, 30354:30354, 30382:30382, 30403:30403, 30424:30424, 30442:30442, 30460:30460, 30478:30478, 30500:30500, 30518:30518, 30536:30536, 30561:30561, 30586:30586, 30604:30604, 30625:30625, 30648:30648, 30671:30671, 30707:30707, 30728:30728, 30749:30749, 30785:30785, 30806:30806, 30827:30827, 30847:30847, 30865:30865, 30893:30893, 30928:30928, 30964:30964, 30997:30997, 31030:31030, 31056:31056, 31090:31090, 31120:31120, 31164:31164, 31242:31242, 31302:31306, 31336:31337, 31362:31362, 31377:31381, 31415:31415, 31449:31449, 31480:31480, 31513:31513, 31542:31542, 31571:31571, 31599:31599, 31629:31629, 31650:31650, 31672:31672, 31696:31696, 31720:31720, 31746:31746, 31769:31769, 31802:31802, 31848:31848, 31893:31893, 31922:31922, 31950:31950, 31965:31965, 31983:31983, 32001:32001, 32019:32019, 32036:32036, 32053:32053, 32073:32073, 32090:32090, 32107:32107, 32127:32127, 32144:32144, 32161:32161, 32184:32184, 32204:32204, 32224:32224, 32247:32247, 32270:32270, 32295:32300, 32324:32324, 32347:32347, 32362:32362, 32384:32384, 32414:32414, 32436:32436, 32458:32458, 32482:32482, 32504:32504, 32526:32526, 32554:32554, 32576:32576, 32598:32598, 32622:32622, 32645:32645, 32672:32672, 32704:32704, 32730:32730, 32752:32752, 32774:32774, 32801:32801, 32824:32824, 32847:32847, 32878:32878, 32906:32906, 32934:32934, 32962:32962, 32990:32990, 33018:33018, 33046:33046, 33073:33073, 33115:33123, 33149:33149, 33177:33177, 33207:33207, 33238:33238, 33268:33268, 33294:33301, 33337:33337, 33363:33363, 33386:33386, 33408:33408, 33431:33431, 33454:33454, 33477:33477, 33500:33500, 33524:33524, 33558:33558, 33587:33587, 33612:33612, 33646:33646, 33675:33675, 33712:33712, 33738:33738, 33764:33764, 33788:33788, 33811:33811, 33839:33839, 33867:33867, 33895:33895, 33916:33916, 33938:33938, 33960:33960, 33988:33988, 34017:34017, 34046:34046, 34080:34080, 34114:34114, 34138:34138, 34163:34172, 34210:34210, 34265:34265, 34330:34330, 34399:34410, 34478:34478, 34528:34528, 34575:34575, 34778:34779, 34946:34946, 35012:35012, 35147:35147, 35474:35474, 35526:35526, 35610:35616, 35639:35639, 35662:35662, 35676:35676, 35731:35731}
    lines = []
    for c in tu.cursor.get_tokens():

        # stop at every comment token
        if c.kind == TokenKind.COMMENT:
            nextlinenumber = c.extent.end.line + 1

            #
            # is this function part of the current header module?
            #
            if  nextlinenumber in function_dict or nextlinenumber in static_functions_dict:

                # uncomment to print the documentation
                #print c.spelling
                thedict = function_dict if nextlinenumber in function_dict else static_functions_dict
                function = thedict[nextlinenumber]
                functionname = fully_qualified(function.referenced)

                #
                # print the whole extent from the beginning to the end.
                #
                #output(functionname, c.extent.start.line, function.extent.end.line)
                lines.append((c.extent.start.line, function.extent.end.line))
    printGaps = False
    if printGaps:
        start = 1
        i = 0
        gaplist = []
        while i < len(lines) - 1:
            _,start = lines[i]
            end,_ = lines[i+1]
            if start + 1 <= end - 1:
                gaplist.append((start + 1, end - 1))
            i += 1

        for (start, end) in gaplist:
            if start <= end:
                print "{}:{},".format(start, end)
    elif len(lines) > 0:
        start,end = lines[0]
        end+= 1
        merged_lines = []
        i = 0
        while i < len(lines) - 1:
            nextstart,nextend = lines[i+1]
            nextend += 1
            if end in gaps and gaps[end] == nextstart - 1:
                end = nextend
            else:
                merged_lines.append((start,end))
                start,end = (nextstart,nextend)
            i += 1
        merged_lines.append((start,end - 1))

        for (start,end) in merged_lines:
            print "{},{}p".format(start,end)
