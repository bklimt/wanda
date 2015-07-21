
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <wand/MagickWand.h>

using std::string;
using std::vector;

DEFINE_string(input, "input.jpg", "input image");
DEFINE_string(output, "output.jpg", "output image");
DEFINE_int32(passes, 1, "number of passes to make");
DEFINE_int32(clusters, 3, "number of clusters");
DEFINE_bool(use_original_cluster_colors, true,
            "whether to use them instead of the average color of the pixels");
DEFINE_bool(dither, true, "whether to dither");
DEFINE_string(cluster_colors, "",
              "the initial colors for each cluster, as comma-separated hex");

#define FATAL(wand) { \
  ExceptionType severity; \
  char *description = MagickGetException(wand, &severity); \
  fprintf(stderr, "%s:%s:%lu %s\n", GetMagickModule(), description); \
  MagickRelinquishMemory(description); \
  exit(-1); \
}

#define CHECK_WAND(action) { \
  MagickBooleanType ok = action; \
  if (ok == MagickFalse) { \
    FATAL(wand); \
  } \
}

namespace {

struct Cluster {
  double original_red;
  double original_green;
  double original_blue;
  double red;
  double green;
  double blue;
  double red_sum;
  double green_sum;
  double blue_sum;
  int count;
};

void SplitString(const string &str, char delim, vector<string> *parts) {
  size_t pos = str.find(delim);
  if (pos == string::npos) {
    parts->push_back(str);
    return;
  }
  parts->push_back(str.substr(0, pos));
  SplitString(str.substr(pos + 1, string::npos), delim, parts);
}

void ParseColor(const string &str, double *r, double *g, double *b) {
  int hr, hg, hb;
  if (sscanf(str.c_str(), "%2x%2x%2x", &hr, &hg, &hb) != 3) {
    LOG(FATAL) << "Unable to parse color: \"" << str << "\".";
  }
  *r = hr / 255.0;
  *g = hg / 255.0;
  *b = hb / 255.0;
}

}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::SetUsageMessage("wanda");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  MagickWandGenesis();
  srand(time(NULL));

  LOG(INFO) << "Passes: " << FLAGS_passes;
  LOG(INFO) << "Clusters: " << FLAGS_clusters;

  MagickWand *wand = NewMagickWand();
  CHECK_WAND(MagickReadImage(wand, FLAGS_input.c_str()));

  vector<Cluster> clusters;
  vector<string> cluster_colors;
  if (FLAGS_cluster_colors != "") {
    SplitString(FLAGS_cluster_colors, ',', &cluster_colors);
    if (cluster_colors.size() != FLAGS_clusters) {
      LOG(FATAL) << cluster_colors.size() << " colors for "
                 << FLAGS_clusters << " clusters.";
    }
  }
  for (int i = 0; i < FLAGS_clusters; ++i) {
    Cluster c;
    if (FLAGS_cluster_colors == "") {
      c.red = rand() / static_cast<double>(RAND_MAX);
      c.green = rand() / static_cast<double>(RAND_MAX);
      c.blue = rand() / static_cast<double>(RAND_MAX);
    } else {
      ParseColor(cluster_colors[i], &c.red, &c.green, &c.blue);
    }
    c.original_red = c.red;
    c.original_green = c.green;
    c.original_blue = c.blue;
    clusters.push_back(c);
  }

  MagickResetIterator(wand);
  while (MagickNextImage(wand) != MagickFalse) {
    size_t height = MagickGetImageHeight(wand);
    if (height == 0) {
      FATAL(wand);
    }

    for (int pass = 0; pass < FLAGS_passes; ++pass) {
      LOG(INFO) << "Pass " << pass << "...";

      // Reset the cluster accumulators.
      for (int c = 0; c < clusters.size(); ++c) {
        clusters[c].red_sum = 0.0;
        clusters[c].green_sum = 0.0;
        clusters[c].blue_sum = 0.0;
        clusters[c].count = 0;
      }
  
      PixelIterator *iterator = NewPixelIterator(wand);
      if (iterator == NULL) {
        FATAL(wand);
      }
  
      for (int y = 0; y < height; ++y) {
        PixelWand **pixels;
        size_t width;
        pixels = PixelGetNextIteratorRow(iterator, &width);
        for (int x = 0; x < width; ++x) {
          double r = PixelGetRed(pixels[x]);
          double g = PixelGetGreen(pixels[x]);
          double b = PixelGetBlue(pixels[x]);
  
          vector<double> inv_distances(clusters.size());

          // Figure out which cluster it's nearest to.
          int closest = 0;
          double min_dist_sq = 0.0;
          double total_inv_dist = 0.0;
          for (int c = 0; c < FLAGS_clusters; ++c) {
            double dist_sq =
                ((clusters[c].red - r) * (clusters[c].red - r)) +
                ((clusters[c].green - g) * (clusters[c].green - g)) +
                ((clusters[c].blue - b) * (clusters[c].blue - b));
            if (c == 0 || dist_sq < min_dist_sq) {
              closest = c;
              min_dist_sq = dist_sq;
              clusters[c].count++;
              clusters[c].red_sum += r;
              clusters[c].green_sum += g;
              clusters[c].blue_sum += b;
            }
            if (FLAGS_dither && (pass == FLAGS_passes - 1)) {
              double dist = sqrt(dist_sq);
              inv_distances[c] = (1.0 / dist) * (1.0 / dist);
              total_inv_dist += inv_distances[c];
            }
          }
 
          if (pass == FLAGS_passes - 1) {
            if (FLAGS_dither) {
              double choice = rand() / static_cast<double>(RAND_MAX);
              for (int c = 0; c < inv_distances.size(); ++c) {
                choice -= (inv_distances[c] / total_inv_dist);
                if (choice <= 0.0) {
                  closest = c;
                  break;
                }
              }
            }
 
            // Set the pixel to the color of the cluster.
            if (FLAGS_use_original_cluster_colors) {
              PixelSetRed(pixels[x], clusters[closest].original_red);
              PixelSetGreen(pixels[x], clusters[closest].original_green);
              PixelSetBlue(pixels[x], clusters[closest].original_blue);
            } else {
              PixelSetRed(pixels[x], clusters[closest].red);
              PixelSetGreen(pixels[x], clusters[closest].green);
              PixelSetBlue(pixels[x], clusters[closest].blue);
            }
          }
        }

        // Write this row of changes back into the image.
        CHECK_WAND(PixelSyncIterator(iterator));
      }

      DestroyPixelIterator(iterator);
  
      for (int c = 0; c < clusters.size(); ++c) {
        if (clusters[c].count > 0) {
          clusters[c].red = clusters[c].red_sum / clusters[c].count;
          clusters[c].green = clusters[c].green_sum / clusters[c].count;
          clusters[c].blue = clusters[c].blue_sum / clusters[c].count;
        } else {
          LOG(WARNING) << "Cluster " << c << " had no pixels.";
        }
      }
    }
  }

  for (int c = 0; c < clusters.size(); ++c) {
    LOG(INFO) << "Cluster " << c;
    LOG(INFO) << "    red: " << clusters[c].red;
    LOG(INFO) << "  green: " << clusters[c].green;
    LOG(INFO) << "   blue: " << clusters[c].blue;
    LOG(INFO) << "  count: " << clusters[c].count;
  }

  CHECK_WAND(MagickWriteImages(wand, FLAGS_output.c_str(), MagickTrue));
  DestroyMagickWand(wand);
  return 0;
}
